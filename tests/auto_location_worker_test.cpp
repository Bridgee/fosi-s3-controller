#include "weather_platform.h"
#undef time
#include "weather.h"
#include "config.h"
#include "weather_model.h"
#include "network_clock.h"
#include "timezone_data.h"
#include <fstream>
#include <iterator>
uint32_t ticks=0;size_t freeMemory=260*1024,largestBlock=140*1024;bool wifi=true,allocationFailure=false;
int postCount=0,httpStatus=200,contentLength=-2;bool transfer=false,stall=false,headerOverflow=false;
std::string responseBody,lastPayload;FakeSerial Serial;FakeWiFi WiFi;time_t fakeEpoch=1793012700;
namespace orbit_clock{bool validUtc=false;std::string selected;unsigned sets=0;
 bool utc_ready(){return validUtc;}Reading read(){Reading r;r.status=validUtc?Status::Locating:Status::Syncing;return r;}
 bool set_location(const char *p,size_t n){assert(orbit_zone::find(p,n));selected.assign(p,n);++sets;return true;}}
int main(int argc,char **argv){using namespace orbit_weather;assert(argc==2);std::ifstream f(argv[1]);std::string forecast((std::istreambuf_iterator<char>(f)),{});
 begin();ticks=16000;responseBody=R"({"success":true,"city":"Paris","latitude":48.85,"longitude":2.35,"timezone":{"id":"Europe/Paris"}})";
 worker_tick();assert(!postCount);orbit_clock::validUtc=true;
 // Real clock's local display would still be Locating, but valid UTC permits HTTPS.
 worker_tick();assert(postCount==1&&orbit_clock::selected=="Europe/Paris");
 responseBody=forecast;worker_tick();
#if ORBIT_WEATHER_ENABLED
 assert(postCount==2&&read().available);assert(lastPayload.find("timezone=Europe/Paris")!=std::string::npos);
#else
 assert(postCount==1&&!read().available); // Auto clock still locates with weather disabled.
#endif
 // New network triggers fresh IP location, not a6h wait. Unknown timezone cannot replace it.
 network_changed();ticks+=60000;responseBody=R"({"success":true,"city":"Bad","latitude":0,"longitude":0,"timezone":{"id":"Unknown/Zone"}})";worker_tick();assert(orbit_clock::sets==1);
 ticks+=60000;responseBody=R"({"success":true,"city":"Sydney","latitude":-33.86,"longitude":151.2,"timezone":{"id":"Australia/Sydney"}})";worker_tick();assert(orbit_clock::selected=="Australia/Sydney"&&orbit_clock::sets==2&&!read().available);
 responseBody=forecast;worker_tick();
#if ORBIT_WEATHER_ENABLED
 assert(read().available&&!strcmp(read().city,"Sydney"));assert(lastPayload.find("timezone=Australia/Sydney")!=std::string::npos);
#endif
 // '+' in an IANA Etc ID is percent-encoded, not interpreted as URL whitespace.
 network_changed();ticks+=60000;responseBody=R"({"success":true,"city":"Test","latitude":0,"longitude":0,"timezone":{"id":"Etc/GMT+5"}})";worker_tick();responseBody=forecast;worker_tick();
#if ORBIT_WEATHER_ENABLED
 assert(lastPayload.find("timezone=Etc/GMT%2B5")!=std::string::npos);
#endif
 puts("PASS: actual shared IP location/timezone worker, UTC bootstrap, invalid response, new-network refresh, same-zone forecast, weather-disabled auto clock");
}
