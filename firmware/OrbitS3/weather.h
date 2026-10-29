#pragma once
#include <stdint.h>
namespace orbit_weather {
enum class Status : uint8_t {Offline,Syncing,Locating,Loading,Ready,Saved,Unavailable};
enum class Icon : uint8_t {Sun,Moon,MostlyClearDay,MostlyClearNight,PartlyCloudy,PartlyCloudyNight,Cloud,Fog,RimeFog,LightDrizzle,Drizzle,HeavyDrizzle,LightFreezingDrizzle,FreezingDrizzle,LightRain,Rain,HeavyRain,LightFreezingRain,FreezingRain,LightSnow,Snow,HeavySnow,SnowGrains,LightShowersDay,LightShowersNight,ShowersDay,ShowersNight,HeavyShowersDay,HeavyShowersNight,SnowShowersDay,SnowShowersNight,HeavySnowShowersDay,HeavySnowShowersNight,Thunder,Hail,HeavyThunder,HeavyHail,Count};
struct Conditions {
    float temperature=0,feels=0,high=0,low=0,wind=0;
    uint32_t observed=0;
    uint8_t humidity=0,rain=0,code=0;
    bool day=true;
};
struct Reading {
    Status status=Status::Locating;
    bool available=false;
    char city[80]={};
    Conditions data;
    uint32_t ageMinutes=0;
};
void begin(); // Queue only; called before the existing background worker starts.
void network_changed(); // Network worker only signals a fresh location after Wi-Fi changes.
void worker_tick(bool defer=false); // Background worker only; never call from LVGL.
Reading read(); // Nonblocking UI copy; no I/O, network or allocation.
Icon icon(uint8_t code,bool day);
const char *description(uint8_t code,bool day);
}
