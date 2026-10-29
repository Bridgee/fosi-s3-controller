#include "weather.h"
#include "config.h"
#if ORBIT_LIVE
#include "weather_protocol.h"
#include "weather_root_ca.h"
#include "network_clock.h"
#include "http_header_budget.h"
#include "bounded_body.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <time.h>
#include <atomic>
namespace orbit_weather {
namespace {
struct Published {Reading value;uint32_t fetchedAt=0;bool failed=false;};
QueueHandle_t updates=nullptr;
Published cached;
Location location;
bool located=false;
uint32_t locatedAt=0;
Schedule geolocation,forecast;
std::atomic<uint32_t> networkVersion(0);
uint32_t observedNetwork=0,locationNetwork=0;
struct Cooldown {uint32_t at=0,delay=0;bool ready(uint32_t now){if(delay && uint32_t(now-at)>=delay)delay=0;return delay==0;}void wait(uint32_t now,uint32_t ms){at=now;delay=ms;}};
Cooldown locationCadence,locationLimit,forecastLimit;
struct Clock {uint32_t now()const{return millis();}void idle()const{vTaskDelay(1);}};
struct PsramAllocator {
    void *allocate(size_t n){return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
    void deallocate(void *p){free(p);}
    void *reallocate(void *p,size_t n){return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
void publish(){if(updates)xQueueOverwrite(updates,&cached);}
bool request(const char *url,JsonDocument &document,int &code) {
    Clock clock;s3::HeaderBudgetClient<WiFiClientSecure,Clock> client(clock);
    client.setCACert(WeatherRootCa);client.setHandshakeTimeout(4);
    HTTPClient http;http.setConnectTimeout(2000);http.setTimeout(1500);http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.useHTTP10(true);
    if(!http.begin(client,url))return false;
    const char *headers[]={"Transfer-Encoding"};http.collectHeaders(headers,1);http.addHeader("Accept","application/json");
    client.armHeaders();code=http.GET();client.finishHeaders();bool valid=false;
    if(code==200 && !client.headerLimitHit() && !http.header("Transfer-Encoding").length() && http.getSize()<=8192){
        auto *body=static_cast<char*>(heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(body){size_t received=0;const auto result=s3::read_body(http.getStream(),[&http]{return http.connected();},clock,body,8192,http.getSize(),received);
            if(result==s3::BodyResult::Ok){const auto error=deserializeJson(document,static_cast<const char*>(body),received,DeserializationOption::NestingLimit(6));valid=!error && !document.overflowed();}
            free(body);
        }
    }
    http.end();return valid;
}
}
void begin() {
    if(ORBIT_WEATHER_ENABLED)updates=xQueueCreate(1,sizeof(Published));
    geolocation.success(millis(),15000); // Let Wi-Fi, S3 discovery and initial artwork settle first.
    cached.value.status=Status::Locating;publish();
}
void network_changed(){networkVersion.fetch_add(1);}
void worker_tick(bool defer) {
    const uint32_t now=millis();
    // Expire waits even offline; elapsed cooldowns must not revive after millis wraps.
    locationCadence.ready(now);locationLimit.ready(now);forecastLimit.ready(now);
    if(WiFi.status()!=WL_CONNECTED)return;
    if(!orbit_clock::utc_ready())return; // Valid certificate time, no insecure bootstrap.
    const uint32_t epoch=uint32_t(time(nullptr));
    const auto requestNetwork=networkVersion.load();
    if(requestNetwork!=observedNetwork){
        observedNetwork=requestNetwork;cached.failed=true;
        // Do not keep restarting the initial settle/failure delay on DHCP flaps.
        if(located)geolocation.success(now,0);
        publish();
    }
    if(cached.value.available && !usable(cached.value.data,epoch,uint32_t(now-cached.fetchedAt))){cached.value.available=false;cached.value.status=Status::Unavailable;publish();}
    if(defer || heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<192*1024 ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<96*1024)return;
    // Both jobs share the artwork worker and release TLS/JSON memory between calls.
    const bool locate=geolocation.due(now);
    if(locate && (!locationCadence.ready(now) || !locationLimit.ready(now)))return;
    if(!locate && (!ORBIT_WEATHER_ENABLED || !located || locationNetwork!=requestNetwork || !forecastLimit.ready(now) || !forecast.due(now) || uint32_t(now-locatedAt)>=24*60*60*1000u))return;
    BasicJsonDocument<PsramAllocator> doc(8192);if(doc.capacity()<8192)return;
    int code=0;bool okay=false;
    if(locate){
        cached.value.status=Status::Locating;publish();
        Location found;
        const bool received=request("https://ipwho.is/?fields=success,message,city,region,latitude,longitude,timezone",doc,code);
        locationCadence.wait(millis(),60000);
        if(code==429)locationLimit.wait(millis(),24*60*60*1000u);
        // A completed old-network response is not a location for the new network.
        if(WiFi.status()!=WL_CONNECTED || requestNetwork!=networkVersion.load()){cached.failed=true;publish();return;}
        okay=received && parse_location(doc.as<JsonVariantConst>(),found) && orbit_clock::set_location(found.timezone,strlen(found.timezone));
        if(okay){
            const bool changed=!located || strcmp(location.city,found.city) || location.latitude!=found.latitude || location.longitude!=found.longitude || strcmp(location.timezone,found.timezone);
            location=found;located=true;locationNetwork=requestNetwork;locatedAt=millis();geolocation.success(millis(),LocationMs);
            if(changed){forecast.success(millis(),0);cached.value.available=false;cached.value.city[0]=0;}
            cached.value.status=Status::Loading;
        }else{geolocation.failure(millis(),code==429?24*60*60*1000u:0);cached.value.status=Status::Unavailable;cached.failed=true;}
    }else{
        cached.value.status=Status::Loading;publish();
        char timezone[192];size_t zoneBytes=0;
        for(const char *p=location.timezone;*p;++p){if(*p=='+'){memcpy(timezone+zoneBytes,"%2B",3);zoneBytes+=3;}else timezone[zoneBytes++]=*p;}timezone[zoneBytes]=0;
        char url[600];const int count=snprintf(url,sizeof(url),"https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f&current=temperature_2m,apparent_temperature,relative_humidity_2m,is_day,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&forecast_days=1&timezone=%s&temperature_unit=celsius&wind_speed_unit=kmh&timeformat=unixtime",double(location.latitude),double(location.longitude),timezone);
        Conditions found;
        const bool received=count>0 && size_t(count)<sizeof(url) && request(url,doc,code);
        if(code==429)forecastLimit.wait(millis(),60*60*1000u);
        if(WiFi.status()!=WL_CONNECTED || requestNetwork!=networkVersion.load()){cached.failed=true;publish();return;}
        okay=received && parse_forecast(doc.as<JsonVariantConst>(),uint32_t(time(nullptr)),found);
        if(okay){cached.value.data=found;memcpy(cached.value.city,location.city,sizeof(location.city));cached.fetchedAt=millis();cached.failed=false;cached.value.available=true;cached.value.status=Status::Ready;forecast.success(millis(),RefreshMs);}
        else{cached.failed=true;cached.value.status=Status::Unavailable;forecast.failure(millis(),code==429?60*60*1000u:0);}
    }
    publish();
    if(ORBIT_DIAGNOSTICS && Serial)Serial.printf("Weather: job=%s ok=%u http=%d duration_ms=%u stack_hwm=%u\n",locate?"location":"forecast",unsigned(okay),code,unsigned(millis()-now),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
}
Reading read() {
    if(!ORBIT_WEATHER_ENABLED){Reading empty;empty.status=Status::Unavailable;return empty;}
    Published value;
    if(!updates || xQueuePeek(updates,&value,0)!=pdTRUE){Reading empty;empty.status=Status::Unavailable;return empty;}
    const bool wifi=WiFi.status()==WL_CONNECTED;
    const bool clock=orbit_clock::utc_ready();
    const uint32_t epoch=uint32_t(time(nullptr));
    if(value.value.available && !usable(value.value.data,epoch,uint32_t(millis()-value.fetchedAt)))value.value.available=false;
    if(value.value.available){value.value.ageMinutes=(epoch-value.value.data.observed)/60;value.value.status=!wifi || !clock || value.failed || uint32_t(millis()-value.fetchedAt)>RefreshMs+60000?Status::Saved:Status::Ready;}
    else if(!wifi)value.value.status=Status::Offline;
    else if(!clock)value.value.status=Status::Syncing;
    else if(value.value.status==Status::Ready)value.value.status=Status::Unavailable;
    return value.value;
}
}
#else
namespace orbit_weather {
void begin(){}
void network_changed(){}
void worker_tick(bool){}
Reading read(){Reading value;value.status=Status::Unavailable;return value;}
}
#endif
