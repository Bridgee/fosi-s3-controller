#include "weather_platform.h"
#undef time
#include "weather.h"
#include "weather_model.h"
#include "network_clock.h"
#include <fstream>
#include <iterator>
uint32_t ticks=0;size_t freeMemory=260*1024,largestBlock=140*1024;bool wifi=true,allocationFailure=false;
int postCount=0,httpStatus=200,contentLength=-2;bool transfer=false,stall=false,headerOverflow=false;
std::string responseBody,lastPayload;FakeSerial Serial;FakeWiFi WiFi;time_t fakeEpoch=1793012700;
namespace orbit_clock{static Status fixture=Status::Ready;Reading read(){Reading r;r.status=fixture;return r;}bool utc_ready(){return fixture==Status::Ready||fixture==Status::Locating;}bool set_location(const char*,size_t){return true;}}
using namespace orbit_weather;
static void tick(bool defer=false){worker_tick(defer);}
int main(int argc,char **argv){
 assert(argc==2);std::ifstream f(argv[1]);std::string forecast((std::istreambuf_iterator<char>(f)),{});
 begin();assert(!read().available);tick();assert(postCount==0);ticks=16000;
 responseBody=R"({"success":true,"city":"Los Angeles","latitude":34.0522,"longitude":-118.2437,"timezone":{"id":"America/Los_Angeles"}})";
 wifi=false;tick();assert(read().status==Status::Offline && postCount==0);wifi=true;
 orbit_clock::fixture=orbit_clock::Status::Syncing;tick();assert(read().status==Status::Syncing && postCount==0);orbit_clock::fixture=orbit_clock::Status::Ready;
 tick(true);assert(postCount==0);freeMemory=100*1024;tick();assert(postCount==0);freeMemory=260*1024;largestBlock=50*1024;tick();assert(postCount==0);largestBlock=140*1024;
 allocationFailure=true;tick();assert(postCount==0);allocationFailure=false;
 tick();assert(postCount==1 && read().status==Status::Loading);assert(lastPayload.find("ipwho.is/?fields=")!=std::string::npos && lastPayload.find("fields=ip")==std::string::npos);
 responseBody=forecast;tick();assert(postCount==2 && read().available && read().status==Status::Ready && !strcmp(read().city,"Los Angeles"));assert(lastPayload.find("latitude=34.05&longitude=-118.24")!=std::string::npos && lastPayload.find("timezone=America/Los_Angeles")!=std::string::npos);
 tick();assert(postCount==2);wifi=false;assert(read().status==Status::Saved && read().available);wifi=true;
 ticks+=RefreshMs;httpStatus=503;tick();assert(postCount==3 && read().available && read().status==Status::Saved);tick();assert(postCount==3);
 ticks+=60000;httpStatus=200;responseBody="{}";tick();assert(postCount==4 && read().status==Status::Saved);
 ticks+=300000;responseBody=forecast;transfer=true;tick();assert(postCount==5 && read().status==Status::Saved);transfer=false;
 ticks+=900000;headerOverflow=true;tick();assert(postCount==6 && read().status==Status::Saved);headerOverflow=false;
 ticks+=1800000;stall=true;const auto start=ticks;tick();assert(ticks-start>=2500 && ticks-start<3000 && postCount==7);stall=false;
 // Cached observations expire even offline, and do not return fake zero values.
 fakeEpoch+=7201;wifi=false;auto value=read();assert(!value.available && value.status==Status::Offline);wifi=true;tick();assert(!read().available);
 ticks+=1800000;fakeEpoch=1793012700;responseBody=forecast;tick();assert(read().available && read().status==Status::Ready);
 // 429 is not retried on every tick; next weather attempt waits at least an hour.
 ticks+=RefreshMs;httpStatus=429;tick();const int requests=postCount;ticks+=1800000;tick();assert(postCount==requests);ticks+=1800000;httpStatus=200;tick();assert(postCount==requests+1);
 // Oversized and truncated bodies must also retain the good cached sample.
 ticks+=RefreshMs;contentLength=9000;tick();assert(read().status==Status::Saved && read().available);
 ticks+=60000;contentLength=int(forecast.size())+20;tick();assert(read().status==Status::Saved && read().available);
 ticks+=300000;contentLength=-1;responseBody=forecast;tick();assert(read().status==Status::Ready && read().available);
 puts("Actual weather worker: TLS/body bounds, offline/cache, memory/defer and request cadence PASS");
}
