#include "time_tools.h"
#include <Preferences.h>
#include <assert.h>
#include <stdio.h>
using namespace orbit_time;
static orbit_clock::Reading wall(uint32_t day,unsigned h,unsigned m){orbit_clock::Reading r;r.status=orbit_clock::Status::Ready;r.dateKey=day;r.hour=h;r.minute=m;return r;}
int main(){
 auto &prefs=fake_preferences::state();begin();assert(!state().config().enabled&&state().config().seconds==300&&saved()&&prefs.writes==0);
 assert(!alarm(24,0,true)&&!alarm(0,60,true));assert(!preset(0)&&!preset(6000));assert(prefs.writes==0);
 assert(alarm(7,30,true));auto n=prefs.writes;assert(alarm(7,30,true)&&prefs.writes==n);
 tick(0,wall(20261027,7,29));assert(!state().alerts());tick(1000,wall(20261027,7,30));assert(state().alerts()==Alarm&&prefs.writes==n+1);dismiss();tick(2000,wall(20261027,7,30));assert(!state().alerts());begin();tick(3000,wall(20261027,7,30));assert(!state().alerts());tick(4000,wall(20261028,7,30));assert(state().alerts()==Alarm);dismiss();
 // Fall-back duplicated local01:30 rings once, not twice. A new date rings again.
 assert(alarm(1,30,true));tick(5000,wall(20261101,1,30));assert(state().alerts()==Alarm);dismiss();tick(6000,wall(20261101,1,0));tick(7000,wall(20261101,1,30));assert(!state().alerts());tick(8000,wall(20261102,1,30));assert(state().alerts()==Alarm);dismiss();
 // Spring-forward02:30 does not exist: no fabricated03:00 alarm.
 assert(alarm(2,30,true));tick(9000,wall(20260308,1,59));tick(10000,wall(20260308,3,0));assert(!state().alerts());tick(11000,wall(20260309,2,30));assert(state().alerts()==Alarm);dismiss();
 auto empty=orbit_clock::Reading{};alarm(0,0,false);assert(preset(3));n=prefs.writes;start(0xfffffff0u);assert(state().timer()==Timer::Running&&!preset(5));tick(1000,empty);assert(state().remaining()==2);pause(1010);unsigned remaining=state().remaining();tick(6000,empty);assert(state().remaining()==remaining&&state().timer()==Timer::Paused);start(6000);tick(7980,empty);assert(state().alerts()==Countdown&&state().remaining()==0);assert(prefs.writes==n);tick(9000,empty);assert(state().alerts()==Countdown);dismiss();assert(state().remaining()==3&&!state().alerts());
 assert(preset(5999));start(10000);tick(6009000,empty);assert(state().alerts()==Countdown);dismiss();
 // Coincident reminders remain visible until acknowledged, with no music action.
 alarm(10,11,true);preset(1);start(500);tick(1500,wall(20261029,10,11));assert(state().alerts()==3);dismiss();assert(!state().alerts()&&state().config().enabled&&state().remaining()==1);
 // Timer's in-flight state is intentionally RAM-only; selected preset persists.
 preset(125);start(10000);tick(12000,empty);begin();assert(state().timer()==Timer::Idle&&state().remaining()==125&&state().config().enabled);
 auto good=prefs.values["orbit-time/settings"];assert(good.size()==88);
 for(unsigned i=0;i<good.size();++i){auto bad=good;bad[i]^=1;prefs.values["orbit-time/settings"]=bad;begin();assert(!state().config().enabled&&state().config().seconds==300);}
 prefs.values["orbit-time/settings"]=good;begin();assert(state().config().seconds==125);
 prefs.failWrite=true;assert(!alarm(9,12,true)&&!saved());prefs.failWrite=false;assert(alarm(9,12,true)&&saved());
 assert(Engine::date(20240229)&&!Engine::date(20250229)&&!Engine::date(20261301)&&!Engine::date(20260431));
 // A new city/timezone follows its local alarm time even if the calendar moved back.
 auto local=wall(20261030,7,30);local.zoneKey=100;alarm(7,30,true);tick(100,local);assert(state().alerts()==Alarm);dismiss();
 local.dateKey=20261029;local.zoneKey=200;tick(200,local);assert(state().alerts()==Alarm);dismiss();begin();tick(300,local);assert(!state().alerts());
 // Migrate the old16-byte record without rewriting it or losing enabled/time/preset.
 auto &bytes=prefs.values["orbit-time/settings"];bytes={1,1,7,30,125,0,0,0,0,0,0,0,0,0,0,0};uint32_t checksum=2166136261u;
 for(unsigned i=0;i<12;++i){checksum^=bytes[i];checksum*=16777619u;}memcpy(bytes.data()+12,&checksum,4);n=prefs.writes;begin();assert(state().config().enabled&&state().config().seconds==125&&saved()&&prefs.writes==n);
  // Legacy same-day alarm stays suppressed after automatic Los Angeles selection.
 uint32_t oldDate=20261030;memcpy(bytes.data()+8,&oldDate,4);checksum=2166136261u;for(unsigned i=0;i<12;++i){checksum^=bytes[i];checksum*=16777619u;}memcpy(bytes.data()+12,&checksum,4);begin();
 uint32_t zone=2166136261u;for(const char *s="America/Los_Angeles";*s;++s){zone^=uint8_t(*s);zone*=16777619u;}
 local=wall(oldDate,7,30);local.zoneKey=zone;tick(400,local);assert(!state().alerts());
 assert(alarm(8,30,true)&&bytes.size()==88);
 // Stored v2 alias keys are canonicalized without changing the20B format/write count.
 bytes={2,1,7,30,125,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};memcpy(bytes.data()+8,&oldDate,4);
 uint32_t alias=2166136261u;for(const char *s="US/Pacific";*s;++s){alias^=uint8_t(*s);alias*=16777619u;}memcpy(bytes.data()+12,&alias,4);checksum=2166136261u;for(unsigned i=0;i<16;++i){checksum^=bytes[i];checksum*=16777619u;}memcpy(bytes.data()+16,&checksum,4);
 n=prefs.writes;begin();assert(state().firedZone()==zone&&saved()&&prefs.writes==n);local=wall(oldDate,7,30);local.zoneKey=zone;tick(500,local);assert(!state().alerts());
 // Four independent alarms and concurrent timers; selection never owns execution.
 begin();for(unsigned i=0;i<Slots;++i){assert(alarm(6+i,10+i,i!=2,i));assert(preset(2+i,i));}
 const auto writes=prefs.writes;for(unsigned i=0;i<Slots;++i)start(1000,i);
 tick(3000,empty);assert(timer_mask()==1&&alerts()==Countdown);assert(state(1).timer()==Timer::Running&&state(3).remaining()==3);
 dismiss();assert(!alerts()&&state(0).remaining()==2&&state(1).timer()==Timer::Running);
 tick(4000,empty);assert(timer_mask()==2);dismiss();pause(4100,2);auto paused=state(2).remaining();tick(6000,empty);assert(timer_mask()==8&&state(2).remaining()==paused);dismiss();assert(prefs.writes==writes);
 start(6000,2);tick(7000,empty);assert(timer_mask()==4);dismiss();
 for(unsigned i=0;i<Slots;++i){assert(alarm(8,15,true,i));}
 tick(8000,wall(20261101,8,15));assert(alarm_mask()==15&&alerts()==Alarm);dismiss();tick(9000,wall(20261101,8,15));assert(!alerts());begin();tick(10000,wall(20261101,8,15));assert(!alerts());
 for(unsigned i=0;i<Slots;++i){assert(state(i).config().enabled&&state(i).config().hour==8&&state(i).config().seconds==2+i);assert(state(i).timer()==Timer::Idle);}
 assert(!alarm(1,0,true,4)&&!preset(1,4));n=prefs.writes;start(0,4);pause(0,4);reset(4);assert(prefs.writes==n);
 // A saved bank cannot partially restore a corrupt second slot.
 auto validBank=prefs.values["orbit-time/settings"];validBank[30]^=1;prefs.values["orbit-time/settings"]=validBank;begin();for(unsigned i=0;i<Slots;++i)assert(!state(i).config().enabled&&state(i).config().seconds==300);
 puts("PASS: actual time engine/settings, daily/DST/midnight/offline gates, NVS no poll writes/checksum/failure/reboot, monotonic rollover countdown/pause/reset/collision/limits; four-slot migration/concurrency/alert masks/independent persistence");
}
