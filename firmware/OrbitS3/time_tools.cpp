#include "time_tools.h"
#include "timezone_data.h"
#include <Preferences.h>
#include <stddef.h>
#include <string.h>
namespace orbit_time {
namespace {
struct LegacyRecord {uint8_t version,enabled,hour,minute;uint16_t seconds,reserved;uint32_t fired,checksum;};
struct Record {uint8_t version,enabled,hour,minute;uint16_t seconds,reserved;uint32_t fired,zone,checksum;};
struct Bank {uint8_t version,count,reserved[2];Record slots[Slots];uint32_t checksum;};
static_assert(sizeof(LegacyRecord)==16,"Legacy clock preferences");
static_assert(sizeof(Record)==20,"Stable slot preferences");
static_assert(sizeof(Bank)==88,"Stable four-slot clock preferences");
Engine engines[Slots];Bank committed={};bool hasCommitted=false;
template<class T>uint32_t hash(const T &r){uint32_t v=2166136261u;auto *p=reinterpret_cast<const uint8_t*>(&r);for(size_t i=0;i<offsetof(T,checksum);++i){v^=p[i];v*=16777619;}return v;}
Record record(unsigned i){auto c=engines[i].config();Record r={};r.version=2;r.enabled=c.enabled;r.hour=c.hour;r.minute=c.minute;r.seconds=c.seconds;r.fired=engines[i].fired();r.zone=engines[i].firedZone();r.checksum=hash(r);return r;}
Bank bank(){Bank b={};b.version=3;b.count=Slots;for(unsigned i=0;i<Slots;++i)b.slots[i]=record(i);b.checksum=hash(b);return b;}
bool valid(const Record &r){return r.version==2 && r.checksum==hash(r) && r.enabled<=1 && r.hour<24 && r.minute<60 && r.seconds>=1 && r.seconds<=5999 && !r.reserved && (!r.fired || Engine::date(r.fired));}
bool commit(){const auto b=bank();if(hasCommitted && !memcmp(&b,&committed,sizeof(b)))return true;Preferences p;if(!p.begin("orbit-time",false))return false;bool ok=p.putBytes("settings",&b,sizeof(b))==sizeof(b);p.end();if(ok){committed=b;hasCommitted=true;}return ok;}
}
void begin(){
 for(auto &engine:engines)engine=Engine{};committed=bank();hasCommitted=true;Preferences p;if(!p.begin("orbit-time",true))return;
 const auto size=p.getBytesLength("settings");Bank b={};bool ok=false;unsigned count=1;
 if(size==sizeof(Bank)){ok=p.getBytes("settings",&b,sizeof(b))==sizeof(b) && b.version==3 && b.count==Slots && !b.reserved[0] && !b.reserved[1] && b.checksum==hash(b);count=Slots;}
 else if(size==sizeof(Record)){ok=p.getBytes("settings",&b.slots[0],sizeof(Record))==sizeof(Record);}
 else if(size==sizeof(LegacyRecord)){
  LegacyRecord old={};ok=p.getBytes("settings",&old,sizeof(old))==sizeof(old) && old.version==1 && old.checksum==hash(old);
  if(ok){auto &v=b.slots[0];v.version=2;v.enabled=old.enabled;v.hour=old.hour;v.minute=old.minute;v.seconds=old.seconds;v.reserved=old.reserved;v.fired=old.fired;
   uint32_t key=2166136261u;for(const char *s="America/Los_Angeles";*s;++s){key^=uint8_t(*s);key*=16777619u;}v.zone=old.fired?(key?key:1):0;v.checksum=hash(v);
  }
 }
 p.end();for(unsigned i=0;i<count;++i)ok=valid(b.slots[i]) && ok;
 if(ok){for(unsigned i=0;i<count;++i){const auto &v=b.slots[i];Config c;c.hour=v.hour;c.minute=v.minute;c.enabled=v.enabled;c.seconds=v.seconds;engines[i].restore(c,v.fired,orbit_zone::normalize_key(v.zone));}committed=bank();}
}
const Engine &state(unsigned slot){return engines[slot<Slots?slot:0];}
bool saved(){const auto b=bank();return hasCommitted && !memcmp(&b,&committed,sizeof(b));}
bool alarm(unsigned h,unsigned m,bool enabled,unsigned slot){return slot<Slots && engines[slot].alarm(h,m,enabled)&&commit();}
bool preset(unsigned seconds,unsigned slot){return slot<Slots && engines[slot].preset(seconds)&&commit();}
void start(uint32_t now,unsigned slot){if(slot<Slots)engines[slot].start(now);}void pause(uint32_t now,unsigned slot){if(slot<Slots)engines[slot].pause(now);}void reset(unsigned slot){if(slot<Slots)engines[slot].reset();}
uint8_t alarm_mask(){uint8_t mask=0;for(unsigned i=0;i<Slots;++i)if(engines[i].alerts()&Alarm)mask|=1u<<i;return mask;}
uint8_t timer_mask(){uint8_t mask=0;for(unsigned i=0;i<Slots;++i)if(engines[i].alerts()&Countdown)mask|=1u<<i;return mask;}
uint8_t alerts(){return (alarm_mask()?Alarm:0)|(timer_mask()?Countdown:0);}
bool due(const orbit_clock::Reading &wall){for(const auto &engine:engines)if(engine.due(wall))return true;return false;}
void dismiss(){for(auto &engine:engines)engine.dismiss();}
void tick(uint32_t now,const orbit_clock::Reading &wall){bool changed=false;for(auto &engine:engines){uint32_t before=engine.fired(),zone=engine.firedZone();engine.tick(now,wall);changed|=engine.fired()!=before || engine.firedZone()!=zone;}if(changed)commit();}
}
