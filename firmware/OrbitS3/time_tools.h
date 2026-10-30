#pragma once
#include "network_clock.h"
#include <stdint.h>
namespace orbit_time {
enum class Timer : uint8_t {Idle,Running,Paused};
enum Alert : uint8_t {None=0,Alarm=1,Countdown=2};
struct Config {uint8_t hour=7,minute=30;bool enabled=false;uint16_t seconds=300;};
class Engine {
    Config config_;Timer timer_=Timer::Idle;uint32_t remaining_=300000,last_=0,fired_=0,firedZone_=0;uint8_t alerts_=0;
    void advance(uint32_t now){if(timer_!=Timer::Running)return;uint32_t elapsed=now-last_;last_=now;if(elapsed>=remaining_){remaining_=0;timer_=Timer::Idle;alerts_|=Countdown;}else remaining_-=elapsed;}
public:
    static bool date(uint32_t key){unsigned year=key/10000,month=key/100%100,day=key%100;static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(year<2024 || year>2099 || month<1 || month>12)return false;unsigned n=days[month-1]+(month==2 && year%4==0);return day>=1 && day<=n;}
    void restore(const Config &c,uint32_t fired,uint32_t zone=0){config_=c;fired_=fired;firedZone_=zone;timer_=Timer::Idle;remaining_=uint32_t(c.seconds)*1000;alerts_=0;}
    bool alarm(unsigned h,unsigned m,bool enabled){if(h>23 || m>59)return false;if(h!=config_.hour || m!=config_.minute)fired_=0;config_.hour=h;config_.minute=m;config_.enabled=enabled;return true;}
    bool preset(unsigned seconds){if(seconds<1 || seconds>5999 || timer_==Timer::Running)return false;config_.seconds=seconds;timer_=Timer::Idle;remaining_=seconds*1000;alerts_&=~Countdown;return true;}
    void start(uint32_t now){if(timer_==Timer::Running)return;if(!remaining_ || timer_==Timer::Idle)remaining_=uint32_t(config_.seconds)*1000;last_=now;timer_=Timer::Running;alerts_&=~Countdown;}
    void pause(uint32_t now){advance(now);if(timer_==Timer::Running)timer_=Timer::Paused;}
    void reset(){timer_=Timer::Idle;remaining_=uint32_t(config_.seconds)*1000;alerts_&=~Countdown;}
    bool due(const orbit_clock::Reading &wall)const{return config_.enabled && wall.status==orbit_clock::Status::Ready && date(wall.dateKey) && (wall.zoneKey!=firedZone_ || wall.dateKey>fired_) && wall.hour==config_.hour && wall.minute==config_.minute;}
    void tick(uint32_t now,const orbit_clock::Reading &wall){advance(now);if(due(wall)){fired_=wall.dateKey;firedZone_=wall.zoneKey;alerts_|=Alarm;}}
    void dismiss(){if(alerts_&Countdown)reset();alerts_=0;}
    Config config()const{return config_;}Timer timer()const{return timer_;}
    unsigned remaining()const{return (remaining_+999)/1000;}uint32_t fired()const{return fired_;}uint32_t firedZone()const{return firedZone_;}uint8_t alerts()const{return alerts_;}
};
constexpr unsigned Slots=4;
void begin();const Engine &state(unsigned slot=0);bool saved();
uint8_t alerts();uint8_t alarm_mask();uint8_t timer_mask();bool due(const orbit_clock::Reading &wall);
bool alarm(unsigned hour,unsigned minute,bool enabled,unsigned slot=0);bool preset(unsigned seconds,unsigned slot=0);
void start(uint32_t now,unsigned slot=0);void pause(uint32_t now,unsigned slot=0);void reset(unsigned slot=0);void dismiss();
void tick(uint32_t now,const orbit_clock::Reading &wall);
}
