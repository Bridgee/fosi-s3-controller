#include "weather_platform.h"
#undef time
#include "weather.h"
#include "network_clock.h"
#include <fstream>
#include <iterator>
uint32_t ticks=16000;size_t freeMemory=260*1024,largestBlock=140*1024;bool wifi=true,allocationFailure=false;
int postCount=0,httpStatus=200,contentLength=-2;bool transfer=false,stall=false,headerOverflow=false;
std::string responseBody,lastPayload;FakeSerial Serial;FakeWiFi WiFi;time_t fakeEpoch=1793012700;
namespace orbit_clock{unsigned sets=0;std::string selected;bool utc_ready(){return true;}Reading read(){Reading r;r.status=Status::Ready;return r;}bool set_location(const char *p,size_t n){selected.assign(p,n);++sets;return true;}}
static const char *paris=R"({"success":true,"city":"Paris","latitude":48.85,"longitude":2.35,"timezone":{"id":"Europe/Paris"}})";
int main(int argc,char **argv){assert(argc==3);using namespace orbit_weather;std::ifstream f(argv[1]);std::string forecast((std::istreambuf_iterator<char>(f)),{});if(!strcmp(argv[2],"initial-flaps"))ticks=0;begin();responseBody=paris;
 if(!strcmp(argv[2],"initial-flaps")){for(unsigned i=0;i<20;++i){ticks+=1000;network_changed();worker_tick();}assert(orbit_clock::sets>=1&&postCount<=2);puts("PASS: initial DHCP flaps settle once");return 0;}
 ticks+=16000;
 if(!strcmp(argv[2],"stale-location")){fake_weather::request_hook()=[]{network_changed();};worker_tick();assert(orbit_clock::sets==0);}
 else if(!strcmp(argv[2],"offline-response")){fake_weather::request_hook()=[]{wifi=false;};worker_tick();assert(orbit_clock::sets==0);}
 else if(!strcmp(argv[2],"rate-limit")){worker_tick();assert(orbit_clock::sets==1);ticks+=60000;network_changed();httpStatus=429;responseBody="{}";worker_tick();const auto limitedAt=ticks;const auto count=postCount;for(unsigned i=0;i<20;++i){ticks+=1000;network_changed();worker_tick();}assert(postCount==count);httpStatus=200;responseBody=paris;ticks+=24*60*60*1000u;worker_tick();assert(postCount==count+1&&orbit_clock::sets==2);
 // After a full millis cycle, an already expired cooldown must not revive.
 ticks=limitedAt+3600000u;network_changed();worker_tick();assert(postCount==count+2);}
 else if(!strcmp(argv[2],"stale-forecast")){worker_tick();assert(orbit_clock::sets==1);responseBody=forecast;fake_weather::request_hook()=[]{network_changed();};worker_tick();assert(!read().available);}
 else {assert(!strcmp(argv[2],"initial-failure"));responseBody="{}";worker_tick();assert(!orbit_clock::sets&&!read().available);const auto count=postCount;worker_tick();assert(postCount==count);}
 puts("PASS: actual location recovery case");
}
