#include "lyrics.h"
#include "config.h"
#if ORBIT_LIVE && ORBIT_LYRICS_ENABLED
#include "lyrics_protocol.h"
#include "lyrics_root_ca.h"
#include "firmware_version.h"
#include "network_clock.h"
#include "bounded_body.h"
#include "http_header_budget.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <new>
namespace orbit_lyrics {
namespace {
struct Request {Track track;uint32_t generation=0,created=0;};
QueueHandle_t requests=nullptr,results=nullptr;
Track current;uint32_t generation=1;std::atomic<uint32_t> latest(1);std::atomic<bool> active(false);
Request job;Track alternate;bool canRelax=false,relaxed=false;uint8_t stage=0;bool retried=false,temporary=false;uint32_t retryAt=0,lastHttp=0;bool hadHttp=false;
Document *cache[2]={};uint8_t cacheNext=0;
struct Negative {uint64_t key=0;uint32_t at=0;};Negative missing[4];uint8_t negativeNext=0;
struct Cooldown {uint32_t at=0,delay=0;bool blocked=false;bool ready(uint32_t now)const{return !blocked&&uint32_t(now-at)>=delay;}};
Cooldown limits[2];
struct Clock {uint32_t now()const{return millis();}void idle()const{vTaskDelay(1);}};
struct PsramAllocator {void *allocate(size_t n){return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}void deallocate(void *p){free(p);}void *reallocate(void*p,size_t n){return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}};
uint64_t key(const Track &track){uint64_t h=1469598103934665603ULL;for(const char *p:{track.title,track.artist,track.album}){do{h^=uint8_t(*p);h*=1099511628211ULL;}while(*p++);}h^=uint32_t(track.duration);return h?h:1;}
void publish(Document *document){
    if(job.generation!=latest.load()){free(document);return;}
    Result old;if(xQueueReceive(results,&old,0)==pdTRUE)free(old.document);
    Result value;value.generation=job.generation;value.document=document;
    if(xQueueSend(results,&value,0)!=pdTRUE)free(document);
}
Document *allocate(){void *p=heap_caps_malloc(sizeof(Document),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);return p?new(p) Document():nullptr;}
void publish_copy(const Document *document){auto *copy=allocate();if(copy)memcpy(copy,document,sizeof(Document));publish(copy);}
bool url_for(Provider p,const Track &track,char *url,size_t capacity){
    const char *base=p==Provider::Lrclib?"https://lrclib.net/api/get?track_name=":"https://api.lrc.cx/jsonapi?title=";
    size_t used=strlen(base);if(used>=capacity)return false;memcpy(url,base,used);
    auto literal=[&](const char *text){size_t n=strlen(text);if(used+n>=capacity)return false;memcpy(url+used,text,n);used+=n;return true;};
    auto encoded=[&](const char *text){const char *hex="0123456789ABCDEF";for(const unsigned char *p=reinterpret_cast<const unsigned char*>(text);*p;++p){bool safe=(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.'||*p=='~';if(used+(safe?1:3)>=capacity)return false;if(safe)url[used++]=char(*p);else{url[used++]='%';url[used++]=hex[*p>>4];url[used++]=hex[*p&15];}}return true;};
    if(!encoded(track.title)||!literal(p==Provider::Lrclib?"&artist_name=":"&artist=")||!encoded(track.artist))return false;
    if(track.album[0]&&(!literal(p==Provider::Lrclib?"&album_name=":"&album=")||!encoded(track.album)))return false;
    if(p==Provider::Lrclib){char duration[32];snprintf(duration,sizeof(duration),"&duration=%d",track.duration);if(!literal(duration))return false;}
    url[used]=0;return true;
}
void limited(Provider provider,const char *header){
    auto &limit=limits[unsigned(provider)];limit.at=millis();uint64_t seconds=0;bool valid=header&&header[0];
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(header?header:"");*p;++p){if(*p<'0'||*p>'9'){valid=false;break;}seconds=seconds*10+(*p-'0');if(seconds>2147483){limit.blocked=true;return;}}
    limit.delay=valid?uint32_t(seconds?seconds:1)*1000:3600000;
}
bool fetch(Provider provider,Document &document,int &status,bool &retryable){
    retryable=false;
    auto *url=static_cast<char*>(heap_caps_malloc(1792,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!url){retryable=true;return false;}
    const Track &lookup=relaxed?alternate:job.track;
    if(!url_for(provider,lookup,url,1792)){free(url);return false;}
    Clock clock;s3::HeaderBudgetClient<WiFiClientSecure,Clock> client(clock);client.setCACert(LyricsRootCa);client.setHandshakeTimeout(4);
    HTTPClient http;http.setConnectTimeout(2000);http.setTimeout(1500);http.setReuse(false);http.useHTTP10(true);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    bool began=http.begin(client,url);free(url);if(!began){retryable=true;return false;}
    const char *headers[]={"Transfer-Encoding","Content-Type","Content-Encoding","Retry-After"};http.collectHeaders(headers,4);
    http.addHeader("Accept","application/json");http.addHeader("Accept-Encoding","identity");
    http.setUserAgent("OrbitS3/" ORBIT_FIRMWARE_VERSION " (personal ESP32-S3 controller; https://lrclib.net/docs)");
    client.armHeaders();status=http.GET();client.finishHeaders();bool okay=false;
    if(status==429||status==503)limited(provider,http.header("Retry-After").c_str());
    const auto encoding=http.header("Content-Encoding");
    if(status==200 && !client.headerLimitHit() && !http.header("Transfer-Encoding").length() &&
       http.header("Content-Type").startsWith("application/json") && (!encoding.length() || encoding=="identity") && http.getSize()<=65536){
        char *body=static_cast<char*>(heap_caps_malloc(65536,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(body){size_t received=0;auto result=s3::read_body(http.getStream(),[&http]{return http.connected();},clock,body,65536,http.getSize(),received,4000);
            if(result!=s3::BodyResult::Ok)retryable=true;
            if(result==s3::BodyResult::Ok){
                StaticJsonDocument<512> filter;
                if(provider==Provider::Lrclib){for(const char *key:{"trackName","artistName","albumName","duration","instrumental","syncedLyrics"})filter[key]=true;}
                else for(const char *key:{"title","artist","lyrics"})filter[0][key]=true;
                BasicJsonDocument<PsramAllocator> doc(8192);
                // Mutable PSRAM input: ArduinoJson keeps temporary strings in this body.
                // parse_lrc copies accepted text before the body is released.
                if(doc.capacity()>=8192){const auto error=deserializeJson(doc,body,received,DeserializationOption::NestingLimit(6),DeserializationOption::Filter(filter));
                    if(!error&&!doc.overflowed())okay=provider==Provider::Lrclib?lrclib_record(doc.as<JsonVariantConst>(),lookup,document,relaxed):lrcapi_records(doc.as<JsonVariantConst>(),lookup,document,relaxed);else retryable=true;}else retryable=true;
            }
            free(body);
        }else retryable=true;
    }
    if(!okay && (status<=0 || status==429 || status>=500))retryable=true;
    http.end();return okay;
}
void complete(Document *document){
    if(document){free(cache[cacheNext]);cache[cacheNext]=document;cacheNext=(cacheNext+1)%2;publish_copy(document);job.generation=0;}
    else if(temporary&&!retried){publish(nullptr);retried=true;stage=0;relaxed=false;temporary=false;retryAt=millis();}
    else{if(!temporary){missing[negativeNext].key=key(job.track);missing[negativeNext].at=millis();negativeNext=(negativeNext+1)%4;}publish(nullptr);job.generation=0;}
}
}
void begin(){requests=xQueueCreate(1,sizeof(Request));results=xQueueCreate(1,sizeof(Result));}
uint32_t observe(const s3::Snapshot &snapshot){
    Track next;const bool valid=make_track(snapshot,next);
    active.store(valid&&snapshot.connected&&snapshot.sourceKnown&&snapshot.playerKnown&&!s3::power_blocks_media(snapshot));
    if(!same_track(next,current)){
        current=next;if(++generation==0)++generation;latest.store(generation);
        Request value;value.track=next;value.generation=generation;value.created=millis();if(requests)xQueueOverwrite(requests,&value);
    }
    return generation;
}
bool receive(Result &result){return results&&xQueueReceive(results,&result,0)==pdTRUE;}
void release(Document *document){free(document);}
bool worker_tick(bool defer){
    if(!requests||!results)return false;
    Request next;if(xQueueReceive(requests,&next,0)==pdTRUE){job=next;stage=0;relaxed=false;canRelax=relaxed_track(job.track,alternate);retried=temporary=false;retryAt=0;}
    if(!job.generation||job.generation!=latest.load()||!job.track.title[0]){job.generation=0;return false;}
    if(!active.load()||defer||WiFi.status()!=WL_CONNECTED||!orbit_clock::utc_ready())return false;
    const uint32_t now=millis();if(uint32_t(now-job.created)<1500 || (retried&&uint32_t(now-retryAt)<60000))return false;
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<192*1024 || heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<96*1024)return false;
    for(auto *entry:cache)if(entry&&same_track(entry->track,job.track)){publish_copy(entry);job.generation=0;return true;}
    for(auto entry:missing)if(entry.key==key(job.track)&&uint32_t(now-entry.at)<30*60*1000){publish(nullptr);job.generation=0;return true;}
    if(hadHttp && uint32_t(now-lastHttp)<500)return false;
    const Provider provider=stage?Provider::LrcApi:Provider::Lrclib;
    if(!limits[unsigned(provider)].ready(now)){temporary=true;if(stage)complete(nullptr);else{stage=1;relaxed=canRelax;}return false;}
    auto *document=allocate();if(!document)return false;
    int status=0;bool retryable=false;bool found=fetch(provider,*document,status,retryable);lastHttp=millis();hadHttp=true;
    if(job.generation!=latest.load()||!active.load()){free(document);return true;}
    if(ORBIT_DIAGNOSTICS&&Serial)Serial.printf("Lyrics: provider=%s matched=%u http=%d duration_ms=%u stack_hwm=%u\n",provider_name(provider),unsigned(found),status,unsigned(millis()-now),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    if(found){document->track=job.track;complete(document);}
    else{free(document);if(retryable)temporary=true;if(stage)complete(nullptr);else if(canRelax&&!relaxed)relaxed=true;else stage=1;}
    return true;
}
}
#else
#include <stdlib.h>
namespace orbit_lyrics {
void begin(){}uint32_t observe(const s3::Snapshot&){return 0;}bool receive(Result&){return false;}void release(Document*d){free(d);}bool worker_tick(bool){return false;}
}
#endif
