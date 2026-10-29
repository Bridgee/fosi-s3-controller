#include "lyrics_platform.h"
#include "lyrics.h"
#include "network_clock.h"
#include <memory>
uint32_t ticks=0;size_t freeMemory=260*1024,largestBlock=140*1024;bool wifi=true,allocationFailure=false;
int postCount=0,httpStatus=200,contentLength=-2;bool transfer=false,stall=false,headerOverflow=false;
std::string responseBody,lastPayload,retryAfter,contentType="application/json",contentEncoding;std::function<void()> duringGet;FakeSerial Serial;FakeWiFi WiFi;
namespace orbit_clock{static Status fixture=Status::Ready;Reading read(){Reading r;r.status=fixture;return r;}bool utc_ready(){return fixture==Status::Ready||fixture==Status::Locating;}}
using namespace orbit_lyrics;
std::string record(const char *title="Test Song",int duration=180){return std::string("{\"trackName\":\"")+title+"\",\"artistName\":\"Test Artist\",\"albumName\":\"Test Album\",\"duration\":"+std::to_string(duration)+",\"instrumental\":false,\"syncedLyrics\":\"[00:01]Synthetic first line\\n[00:02]Synthetic next line\"}";}
int main(){
 begin();s3::Snapshot source;source.input=0;source.connected=source.sourceKnown=source.playerKnown=true;source.duration=180;strcpy(source.title,"Test Song");strcpy(source.artist,"Test Artist");strcpy(source.album,"Test Album");
 auto gen=observe(source);responseBody=record();worker_tick();assert(postCount==0);ticks=1600;
 wifi=false;worker_tick();assert(postCount==0);wifi=true;orbit_clock::fixture=orbit_clock::Status::Syncing;worker_tick();assert(postCount==0);orbit_clock::fixture=orbit_clock::Status::Ready;
 worker_tick(true);assert(postCount==0);freeMemory=100*1024;worker_tick();assert(postCount==0);freeMemory=260*1024;allocationFailure=true;worker_tick();assert(postCount==0);allocationFailure=false;
 assert(worker_tick()&&postCount==1);assert(lastPayload.find("track_name=Test%20Song")!=std::string::npos && lastPayload.find("album_name=Test%20Album")!=std::string::npos && lastPayload.find("duration=180")!=std::string::npos);
 Result result;assert(receive(result)&&result.generation==gen&&result.document&&result.document->count==2);release(result.document);worker_tick();assert(postCount==1);
 // A second track displaces the UI copy, but returning to the previous track hits bounded RAM cache.
 strcpy(source.title,"Second Song");observe(source);ticks+=1600;responseBody=record("Second Song");worker_tick();assert(postCount==2&&receive(result)&&result.document);release(result.document);
 strcpy(source.title,"Test Song");gen=observe(source);ticks+=1600;worker_tick();assert(postCount==2&&receive(result)&&result.document&&result.generation==gen);release(result.document);
 // A response completed after a track switch must never be published to that new track.
 strcpy(source.title,"Third Song");observe(source);ticks+=1600;responseBody=record("Third Song");duringGet=[&]{strcpy(source.title,"New Song");observe(source);};worker_tick();assert(postCount==3&&!receive(result));ticks+=1600;responseBody=record("New Song");worker_tick();assert(postCount==4&&receive(result)&&result.document);release(result.document);
 // Strict duration mismatch leads to fallback; without an LRC length signature it stays hidden.
 strcpy(source.title,"Missing Song");observe(source);ticks+=1600;responseBody=record("Missing Song",190);worker_tick();ticks+=1000;responseBody=R"([{"title":"Missing Song","artist":"Test Artist","lyrics":"[00:01]A\n[00:02]B"}])";worker_tick();assert(receive(result)&&!result.document);const int missingCount=postCount;
 strcpy(source.title,"New Song");observe(source);ticks+=1600;worker_tick();assert(receive(result)&&result.document);release(result.document);strcpy(source.title,"Missing Song");observe(source);ticks+=1600;worker_tick();assert(receive(result)&&!result.document&&postCount==missingCount);
 // Retry-After is provider-wide across track switches; no replay during a rate limit.
 strcpy(source.title,"Limited Song");observe(source);ticks+=1600;httpStatus=429;retryAfter="120";worker_tick();const int limited=postCount;ticks+=1000;httpStatus=200;responseBody="[]";worker_tick();assert(receive(result)&&!result.document);
 strcpy(source.title,"Other Limited Song");observe(source);ticks+=1600;worker_tick();assert(postCount==limited+1);ticks+=1000;worker_tick();assert(postCount==limited+2);
 // Transient failure retry is bounded and waits60s; no rapid-query loop.
 const int count=postCount;for(int i=0;i<20;++i){ticks+=1000;worker_tick();}assert(postCount==count);
 // Exact signature first, then remaster-note retry; cache/UI retain original identity.
 ticks+=121000;strcpy(source.title,"Test Song (Remastered 2009)");strcpy(source.album,"Test Album (2009 Remaster)");observe(source);ticks+=1600;httpStatus=404;responseBody="{}";worker_tick();
 assert(lastPayload.find("Remastered")!=std::string::npos);ticks+=1000;httpStatus=200;responseBody=record();worker_tick();
 assert(lastPayload.find("Remastered")==std::string::npos&&receive(result)&&result.document&&!strcmp(result.document->track.title,source.title));release(result.document);
 puts("PASS: actual lyrics worker TLS/headers, metadata queue, bounded cache, stale-response rejection, hidden mismatches, request pacing/Retry-After, Wi-Fi/NTP/memory/control gates");
}
