#include "time_tools.h"
#include "timezone_data.h"
#include <Preferences.h>
#include <stddef.h>
#include <string.h>
namespace orbit_time {
namespace {
struct LegacyRecord {uint8_t version,enabled,hour,minute;uint16_t seconds,reserved;uint32_t fired,checksum;};
struct Record {uint8_t version,enabled,hour,minute;uint16_t seconds,reserved;uint32_t fired,zone,checksum;};
static_assert(sizeof(LegacyRecord)==16,"Legacy clock preferences");
static_assert(sizeof(Record)==20,"Stable clock preferences");
Engine engine;Record committed={};bool hasCommitted=false;
template<class T>uint32_t hash(const T &r){uint32_t v=2166136261u;auto *p=reinterpret_cast<const uint8_t*>(&r);for(size_t i=0;i<offsetof(T,checksum);++i){v^=p[i];v*=16777619;}return v;}
Record record(){auto c=engine.config();Record r={};r.version=2;r.enabled=c.enabled;r.hour=c.hour;r.minute=c.minute;r.seconds=c.seconds;r.fired=engine.fired();r.zone=engine.firedZone();r.checksum=hash(r);return r;}
bool commit(){const auto r=record();if(hasCommitted && !memcmp(&r,&committed,sizeof(r)))return true;Preferences p;if(!p.begin("orbit-time",false))return false;bool ok=p.putBytes("settings",&r,sizeof(r))==sizeof(r);p.end();if(ok){committed=r;hasCommitted=true;}return ok;}
}
void begin(){
 engine=Engine{};committed=record();hasCommitted=true;Preferences p;if(!p.begin("orbit-time",true))return;
 const auto size=p.getBytesLength("settings");Record r={};bool valid=false;
 if(size==sizeof(r)){valid=p.getBytes("settings",&r,sizeof(r))==sizeof(r) && r.version==2 && r.checksum==hash(r);}
 else if(size==sizeof(LegacyRecord)){
  LegacyRecord old={};valid=p.getBytes("settings",&old,sizeof(old))==sizeof(old) && old.version==1 && old.checksum==hash(old);
  if(valid){r.version=2;r.enabled=old.enabled;r.hour=old.hour;r.minute=old.minute;r.seconds=old.seconds;r.reserved=old.reserved;r.fired=old.fired;
   // Old firmware always used Los Angeles. Preserve same-day suppression there.
   uint32_t key=2166136261u;for(const char *s="America/Los_Angeles";*s;++s){key^=uint8_t(*s);key*=16777619u;}r.zone=old.fired?(key?key:1):0;
  }
 }
 p.end();valid=valid && r.enabled<=1 && r.hour<24 && r.minute<60 && r.seconds>=1 && r.seconds<=5999 && !r.reserved && (!r.fired || Engine::date(r.fired));
 if(valid){Config c;c.hour=r.hour;c.minute=r.minute;c.enabled=r.enabled;c.seconds=r.seconds;engine.restore(c,r.fired,orbit_zone::normalize_key(r.zone));committed=record();}
}
const Engine &state(){return engine;}bool saved(){const auto r=record();return hasCommitted && !memcmp(&r,&committed,sizeof(r));}
bool alarm(unsigned h,unsigned m,bool enabled){return engine.alarm(h,m,enabled)&&commit();}
bool preset(unsigned seconds){return engine.preset(seconds)&&commit();}
void start(uint32_t now){engine.start(now);}void pause(uint32_t now){engine.pause(now);}void reset(){engine.reset();}void dismiss(){engine.dismiss();}
void tick(uint32_t now,const orbit_clock::Reading &wall){uint32_t before=engine.fired(),zone=engine.firedZone();engine.tick(now,wall);if(engine.fired()!=before || engine.firedZone()!=zone)commit();}
}
