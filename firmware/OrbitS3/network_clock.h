#pragma once
#include <stdint.h>
#include <stddef.h>
namespace orbit_clock {
enum class Status : uint8_t {Offline,Syncing,Ready,Locating};
struct Reading {
    Status status=Status::Offline;
    uint8_t hour=0,minute=0,second=0,day=0,month=0,weekday=0;
    uint16_t year=0;uint32_t epoch=0,dateKey=0,zoneKey=0;
    char time[9]={},date[24]={},zone[8]={};
};
// Start once on the network worker. SNTP is asynchronous; no UI wait or task.
void begin();
// UTC sync is sufficient for HTTPS; location is not required (no bootstrap cycle).
bool utc_ready();
// Same validated IP location as weather. Atomic immutable-rule selection, no TZ mutation.
bool set_location(const char *iana,size_t bytes);
// Nonblocking UI-thread read; only displays time after this boot's SNTP callback
// and while Wi-Fi is connected. S3 connectivity is independent.
Reading read();
// Previously SNTP-synchronized RTC remains usable for alarms across Wi-Fi loss.
Reading trusted_read();
}
