#include "iphone_trigger.h"
#if ORBIT_LIVE && ORBIT_IPHONE_TRIGGER_ENABLED
#if !defined(ORBIT_BUILD_CHECK) && __has_include("iphone_trigger_local.h")
#include "iphone_trigger_local.h"
#endif
#ifndef ORBIT_BARK_DEVICE_KEY
#define ORBIT_BARK_DEVICE_KEY ""
#endif
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "network_clock.h"
#include "http_header_budget.h"
#include "bounded_body.h"
#include "bark_root_ca.h"

namespace orbit_iphone {
namespace {
std::atomic<bool> requested(false),inFlight(false);
std::atomic<LaunchState> outcome(LaunchState::Ready);
std::atomic<uint32_t> finishedAt(0);
uint32_t queuedAt=0; // Published before requested.store; unchanged until busy clears.
uint32_t lastRequest=0;bool hadRequest=false; // UI-thread-only cooldown.
#if ORBIT_IPHONE_SERIAL_TEST
char command[32]={};uint8_t commandSize=0;bool overflow=false;
#endif
struct Clock {uint32_t now()const{return millis();}void idle()const{vTaskDelay(1);}};
bool key_valid() {
    const char *key=ORBIT_BARK_DEVICE_KEY;size_t n=strlen(key);
    if(n<8 || n>128)return false;
    for(size_t i=0;i<n;++i)if(!((key[i]>='a'&&key[i]<='z')||(key[i]>='A'&&key[i]<='Z')||(key[i]>='0'&&key[i]<='9')||key[i]=='_'||key[i]=='-'))return false;
    return true;
}
#if ORBIT_IPHONE_SERIAL_TEST
void print_status() {
    Serial.printf("IPHONE status wifi=%u clock=%u configured=%u busy=%u uptime_ms=%u\n",
        unsigned(WiFi.status()==WL_CONNECTED),unsigned(orbit_clock::utc_ready()),
        unsigned(key_valid()),unsigned(inFlight.load()),unsigned(millis()));
}
#endif
const char *post_once(int &httpCode) {
    Clock clock;
    s3::HeaderBudgetClient<WiFiClientSecure,Clock> client(clock);
    client.setCACert(BarkRootCa);client.setHandshakeTimeout(4);
    HTTPClient http;
    http.setConnectTimeout(2000);http.setTimeout(1500);
    http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.useHTTP10(true);
    if(!http.begin(client,"https://api.day.app/push"))return "not_sent";
    const char *headers[]={"Transfer-Encoding"};http.collectHeaders(headers,1);
    http.addHeader("Content-Type","application/json; charset=utf-8");
    http.addHeader("Accept","application/json");
    char payload[384];const uint32_t id=esp_random();
    const int count=snprintf(payload,sizeof(payload),
        "{\"device_key\":\"%s\",\"title\":\"S3 Start Test\",\"body\":\"FOSI_S3_START\\nBoard %08x\",\"action\":\"none\",\"level\":\"active\"}",
        ORBIT_BARK_DEVICE_KEY,unsigned(id));
    if(count<0 || size_t(count)>=sizeof(payload)){http.end();return "not_sent";}
    Serial.printf("IPHONE event=sending id=%08x\n",unsigned(id));
    client.armHeaders();httpCode=http.POST(reinterpret_cast<uint8_t*>(payload),size_t(count));
    client.finishHeaders(); // Never follow redirects or retry a possibly delivered POST.
    const char *result="uncertain";
    if(httpCode==200 && !client.headerLimitHit() && !http.header("Transfer-Encoding").length() && http.getSize()<=1024){
        char body[1025];size_t received=0;
        const auto read=s3::read_body(http.getStream(),[&http]{return http.connected();},clock,body,1024,http.getSize(),received);
        if(read==s3::BodyResult::Ok){
            StaticJsonDocument<256> reply;
            const auto error=deserializeJson(reply,static_cast<const char*>(body),received,DeserializationOption::NestingLimit(4));
            if(!error && !reply.overflowed() && reply["code"].is<int>())result=reply["code"].as<int>()==200?"accepted":"rejected";
        }
    }else if(httpCode>=400 && httpCode<500 && httpCode!=408)result="rejected";
    http.end();return result;
}
}
bool busy(){return inFlight.load();}
LaunchState state() {
    if(!key_valid())return LaunchState::Unconfigured;
    if(inFlight.load())return LaunchState::Sending;
    const auto now=millis();
    if(hadRequest && uint32_t(now-lastRequest)<20000){
        if(uint32_t(now-finishedAt.load())<4000)return outcome.load();
        return LaunchState::Cooldown;
    }
    if(WiFi.status()!=WL_CONNECTED)return LaunchState::Offline;
    if(!orbit_clock::utc_ready())return LaunchState::Syncing;
    return LaunchState::Ready;
}
bool start() {
    const uint32_t now=millis();bool expected=false;
    if((hadRequest && uint32_t(now-lastRequest)<20000) || !inFlight.compare_exchange_strong(expected,true))return false;
    hadRequest=true;lastRequest=queuedAt=now;requested.store(true);return true;
}
void serial_tick() {
#if ORBIT_IPHONE_SERIAL_TEST
    // A bounded input line cannot stall UI, grow heap or execute arbitrary input.
    for(unsigned n=0;n<32 && Serial.available();++n){
        const int c=Serial.read();if(c<0)break;if(c=='\r')continue;
        if(c!='\n' && (c<32 || c>126)){overflow=true;continue;}
        if(c=='\n'){
            command[commandSize]=0;
            if(!overflow){if(!strcmp(command,"IPHONE_STATUS"))print_status();else if(!strcmp(command,"IPHONE_START"))Serial.println(start()?"IPHONE event=queued":"IPHONE rejected=busy_or_cooldown");}
            commandSize=0;overflow=false;
        }else if(commandSize+1<sizeof(command))command[commandSize++]=char(c);
        else overflow=true;
    }
#endif
}
void worker_tick() {
    if(!requested.exchange(false))return;
    const uint32_t started=millis();const char *result=nullptr;int httpCode=0;
    const size_t before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(uint32_t(started-queuedAt)>5000)result="expired_not_sent";
    else if(!key_valid())result="configuration_not_sent";
    else if(WiFi.status()!=WL_CONNECTED)result="wifi_not_sent";
    else if(!orbit_clock::utc_ready())result="clock_not_sent";
    else if(before<128*1024 || heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<64*1024)result="memory_not_sent";
    else result=post_once(httpCode);
    Serial.printf("IPHONE result=%s http=%d duration_ms=%u internal_before=%u internal_after=%u stack_hwm=%u\n",
        result,httpCode,unsigned(millis()-started),unsigned(before),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    outcome.store(!strcmp(result,"accepted")?LaunchState::Sent:!strcmp(result,"uncertain")?LaunchState::Uncertain:LaunchState::Failed);
    finishedAt.store(millis());inFlight.store(false);
}
}
#else
namespace orbit_iphone {
bool busy(){return false;}
LaunchState state(){return LaunchState::Unconfigured;}
bool start(){return false;}
void serial_tick(){}
void worker_tick(){}
}
#endif
