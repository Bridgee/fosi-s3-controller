"""Exercise the actual media worker loop using fake queues/clock; never downloads."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'firmware/OrbitS3/s3_client.cpp').read_text();a=s.index('void artworkWorker(void *) {');b=s.index('\nvoid worker(void *) {',a);actual=s[a:b]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <cstdio>
namespace orbit_intro_async {struct MediaLease {explicit operator bool()const{return true;}};}
struct Stop{};struct Flag{bool value;bool load()const{return value;}};
struct ArtworkRequest{char url[512]={};uint32_t revision=0;};
uint32_t clockNow=0,started=0,stopAt=900000;bool fresh=true,replacement=false,replaced=false,successOnSecond=false;
int requests=1,artworkRequests=1,commands=2,volumes=3;constexpr int pdTRUE=1;
#define ORBIT_ARTWORK_ENABLED 1
#define pdMS_TO_TICKS(n) (n)
Flag online{true},powerInFlight{false};std::vector<uint32_t> calls,revisions;
uint32_t millis(){return clockNow;}
int xQueueReceive(int q,ArtworkRequest *job,unsigned wait){
 assert(q==requests);if(uint32_t(clockNow-started)>=stopAt)throw Stop{};
 if(fresh || (replacement&&!replaced&&uint32_t(clockNow-started)>=16000)){*job=ArtworkRequest{};job->url[0]='x';job->revision=fresh?1:2;if(!fresh)replaced=true;fresh=false;return pdTRUE;}
 clockNow+=wait;return 0;
}
unsigned uxQueueMessagesWaiting(int){return 0;}
namespace orbit_iphone{bool busy(){return false;}}
namespace orbit_lyrics{bool worker_tick(bool){return false;}}
namespace orbit_weather{void worker_tick(bool){}}
bool downloadArtwork(const ArtworkRequest &job){calls.push_back(clockNow);revisions.push_back(job.revision);return successOnSecond&&calls.size()==2;}
'''+actual+r'''
void run(){try{artworkWorker(nullptr);}catch(const Stop&) {}}
int main(){
 run();const std::vector<uint32_t> expected={0,15000,45000,105000,225000,525000,825000};assert(calls==expected);
 clockNow=0;fresh=true;replacement=true;replaced=false;calls.clear();revisions.clear();stopAt=50000;run();
 assert(calls.size()>=4&&calls[0]==0&&calls[1]==15000&&calls[2]==16000&&calls[3]==31000);
 assert(revisions[0]==1&&revisions[2]==2);
 clockNow=started=0xfffff000u;stopAt=900000;fresh=true;replacement=false;calls.clear();revisions.clear();run();
 for(auto &v:calls)v-=started;assert(calls==expected);
 clockNow=started=0;fresh=true;successOnSecond=true;calls.clear();revisions.clear();run();assert(calls.size()==2);
 puts("PASS: actual media loop failed-cover backoff capped at5min; new artwork immediate, success stops retry, millis wrap");
}
'''
with tempfile.TemporaryDirectory(prefix='orbit-artwork-retry-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
