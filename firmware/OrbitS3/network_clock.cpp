#include "network_clock.h"
#include "config.h"
#include "timezone_data.h"
#include <string.h>
#if ORBIT_LIVE
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <atomic>
#include <time.h>
#include <stdio.h>
namespace orbit_clock {
namespace {
std::atomic<bool> initialized(false),synchronized(false);
std::atomic<uint16_t> selectedZone(0);
bool valid_epoch(time_t value) {
    const int64_t seconds=static_cast<int64_t>(value);
    return seconds>=1704067200LL && seconds<4102444800LL; // 2024..2099
}
void on_sync(struct timeval *value) {
    // TCP/IP callback: publish only a flag, never call LVGL or allocate.
    synchronized.store(value && valid_epoch(value->tv_sec));
}
}
void begin() {
    synchronized.store(false);initialized.store(false);selectedZone.store(0);
    sntp_set_time_sync_notification_cb(on_sync);
    configTzTime("UTC0",ORBIT_NTP_SERVER1,ORBIT_NTP_SERVER2);
    // Keep the SDK clock/TLS in UTC; local conversion uses immutable rules below.
    initialized.store(true);
}
bool utc_ready(){return initialized.load() && synchronized.load() && valid_epoch(time(nullptr));}
bool set_location(const char *iana,size_t bytes){const auto id=orbit_zone::find(iana,bytes);if(!id)return false;selectedZone.store(id);return true;}
Reading trusted_read() {
    Reading result;
    result.status=Status::Syncing;
    const time_t now=time(nullptr);
    if(!initialized.load() || !synchronized.load() || !valid_epoch(now))return result;
    const auto id=selectedZone.load();
    if(!id){result.status=Status::Locating;return result;}
    orbit_zone::Value rule;if(!orbit_zone::at(id,uint32_t(now),rule))return result;
    const int64_t shifted=int64_t(now)+rule.offset;const time_t localEpoch=time_t(shifted);struct tm local={};
    if(int64_t(localEpoch)!=shifted || !gmtime_r(&localEpoch,&local))return result;
    if(!strftime(result.time,sizeof(result.time),"%H:%M:%S",&local) ||
       !strftime(result.date,sizeof(result.date),"%a, %b %d, %Y",&local)){Reading unavailable;unavailable.status=Status::Syncing;return unavailable;}
    memcpy(result.zone,rule.abbreviation,sizeof(result.zone));result.zoneKey=orbit_zone::key(id);
    result.hour=uint8_t(local.tm_hour);result.minute=uint8_t(local.tm_min);
    result.second=uint8_t(local.tm_sec);result.day=uint8_t(local.tm_mday);
    result.year=uint16_t(local.tm_year+1900);result.month=uint8_t(local.tm_mon+1);result.weekday=uint8_t(local.tm_wday);
    result.epoch=uint32_t(now);result.dateKey=uint32_t(result.year)*10000+uint32_t(result.month)*100+result.day;
    result.status=Status::Ready;return result;
}
Reading read(){return WiFi.status()==WL_CONNECTED?trusted_read():Reading{};}
}
#else
namespace orbit_clock {
void begin() {}
bool utc_ready(){return false;}bool set_location(const char*,size_t){return false;}
Reading trusted_read(){return Reading{};}
Reading read() {return Reading{};} // Demo never invents a synchronized time.
}
#endif
