#include "intro_ai_runtime.h"
#include "intro_async_platform.h"
#include "intro_ai_esp32.h"
#include "intro_ai_gate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <string>
#include <fstream>
#include <iterator>
#include <functional>
#include <string.h>
namespace orbit_intro_platform {
namespace fake {unsigned allocations=0,frees=0,tasks=0,notifications=0,failAllocation=0;bool failTask=false,allow=true;size_t liveBytes=0;}
static uint64_t tick=100;
uint64_t clock(){return tick;}
void *allocate(size_t n){++fake::allocations;if(fake::failAllocation==fake::allocations)return nullptr;auto *p=static_cast<size_t*>(malloc(n+sizeof(size_t)));if(!p)return nullptr;*p=n;fake::liveBytes+=n;return p+1;}
void dispose(void *p){if(p){auto *n=static_cast<size_t*>(p)-1;fake::liveBytes-=*n;++fake::frees;free(n);}}
bool admitted(bool){return fake::allow;}
bool start(void(*)(void*),void*,unsigned n,Task &task){assert(n==8192||n==16384);if(fake::failTask)return false;++fake::tasks;task=reinterpret_cast<void*>(1);return true;}
void notify(Task){++fake::notifications;}
void wait(bool){assert(false);}
}
#if !ORBIT_INTRO_MOCK
namespace orbit_intro_ai {
static unsigned factories=0;static bool supply=false;
class FakeChannel:public SecureChannel {
public:
    std::string response,sent;size_t offset=0;unsigned opens=0,closes=0;bool live=false,stall=false;uint64_t deadlineSeen=0;std::function<void()> opening;
    uint64_t nowMs()const override{return orbit_intro_platform::clock();}uint64_t utcSeconds()const override{return 1793214000;}
    Outcome readiness()const override{return Outcome::Idle;}Outcome failure()const override{return Outcome::TlsError;}
    Open open(const ApiSettings &,uint64_t deadline)override{deadlineSeen=deadline;++opens;offset=0;live=true;if(opening){auto f=opening;opening=nullptr;f();}return Open::Ready;}
    int write(const uint8_t *p,size_t n)override{sent.append(reinterpret_cast<const char*>(p),n);return int(n);}
    int available()override{return stall?0:int(response.size()-offset);}int read(uint8_t *p,size_t n)override{n=std::min(n,response.size()-offset);memcpy(p,response.data()+offset,n);offset+=n;return int(n);}
    bool connected()override{return live&&(stall||offset<response.size());}void close()override{++closes;live=false;}
};
static FakeChannel *fakeChannel=nullptr;
SecureChannel *createEsp32Channel(){++factories;if(!supply)return nullptr;fakeChannel=new FakeChannel;return fakeChannel;}
void destroyEsp32Channel(SecureChannel *p){delete p;fakeChannel=nullptr;}
}
#if ORBIT_AI_TRANSPORT_CHECK
extern "C" uintptr_t orbit_ai_runtime_link_probe();
#endif
#endif
int main(int argc,char **argv){
    (void)argc;(void)argv;using namespace orbit_intro_runtime;using namespace orbit_intro_platform;using namespace orbit_intro_async;
#if ORBIT_INTRO_MOCK
    fake::allow=false;assert(!client()&&startupOutcome()==Outcome::ResourceLimited&&fake::allocations==0);fake::allow=true;
    fake::failAllocation=1;assert(!client()&&fake::liveBytes==0&&fake::tasks==0);
    fake::failAllocation=fake::allocations+2;assert(!client()&&fake::liveBytes==0&&fake::tasks==0);
    fake::failAllocation=0;fake::failTask=true;assert(!client()&&fake::liveBytes==0&&fake::tasks==0);fake::failTask=false;
    auto *ui=client();assert(ui&&fake::tasks==1);assert(client()==ui&&fake::tasks==1);const size_t bytes=fake::liveBytes;assert(bytes<100*1024);
    Snapshot s;assert(capture(s,Topic::Song,"Song","Artist","Release",200));assert(ui->begin(s,nowMs())==Outcome::Pending);testPump();tick+=1500;testPump();ui->tick(nowMs());assert(ui->outcome()==Outcome::Ready);
    ui->cancel();assert(ui->begin(s,nowMs())==Outcome::Ready&&ui->fromCache());ui->trackChanged();tick+=2000;assert(ui->begin(s,nowMs())==Outcome::Pending);testPump();
    testReset();assert(fake::liveBytes==0);printf("PASS: actual runtime + synthetic platform; one lazy task, allocation/task/admission failure rollback, queue-driven sample, cache/track cleanup; PSRAM objects=%zu bytes, fake stack excluded; no sockets/device tasks\n",bytes);
#else
    assert(!client()&&startupOutcome()==Outcome::Disabled);assert(fake::allocations==0&&fake::tasks==0&&orbit_intro_ai::factories==0);
#if ORBIT_AI_TRANSPORT_CHECK
    (void)orbit_ai_runtime_link_probe();assert(fake::tasks==0&&orbit_intro_ai::factories==0);
#endif
    puts("PASS: compile-check API runtime cannot start tasks, allocate sessions, read private keys or create channels, even with enable macro supplied");
    // Host-only injected fake channel runs the actual RealSession -> worker -> UI path.
    assert(argc==2);std::ifstream file(argv[1]);std::string body((std::istreambuf_iterator<char>(file)),{});assert(!body.empty());
    orbit_intro_ai::supply=true;ApiSettings settings;settings.enabled=true;settings.key="synthetic_test_key_only_123456";settings.caPem="-----BEGIN CERTIFICATE-----\nSYNTHETIC\n-----END CERTIFICATE-----";
    fake::failAllocation=fake::allocations+1;assert(!testRealClient(settings)&&fake::liveBytes==0&&orbit_intro_ai::fakeChannel==nullptr&&fake::tasks==0);
    fake::failAllocation=fake::allocations+2;assert(!testRealClient(settings)&&fake::liveBytes==0&&orbit_intro_ai::fakeChannel==nullptr&&fake::tasks==0);
    fake::failAllocation=0;fake::failTask=true;assert(!testRealClient(settings)&&fake::liveBytes==0&&orbit_intro_ai::fakeChannel==nullptr&&fake::tasks==0);fake::failTask=false;
    auto *ui=testRealClient(settings);assert(ui&&fake::tasks==1);const auto bytes=fake::liveBytes;assert(bytes<100*1024);
    auto *channel=orbit_intro_ai::fakeChannel;channel->response="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    Snapshot song;assert(capture(song,Topic::Song,"Example Song","Example Artist","Example Release",250));
    const auto originalDeadline=nowMs()+DeadlineMs;
    assert(ui->begin(song,nowMs())==Outcome::Pending&&channel->opens==0);
    tick+=12000; // no networking on UI selection
    for(int i=0;i<100&&ui->outcome()==Outcome::Pending;++i){testPump();ui->tick(nowMs());tick+=20;}
    assert(channel->deadlineSeen==originalDeadline);
    assert(ui->outcome()==Outcome::Ready&&channel->opens==1);ui->cancel();assert(ui->begin(song,nowMs())==Outcome::Ready&&ui->fromCache()&&channel->opens==1);
    // Exercise the real session/HTTPS owner, not only MockSession: cancel
    // another topic in flight and immediately reuse the completed Song cache.
    for(Topic other:{Topic::Artist,Topic::Release}){
        tick+=2000;Snapshot pendingTopic=song;pendingTopic.topic=other;
        channel->stall=true;channel->sent.clear();
        assert(ui->begin(pendingTopic,nowMs())==Outcome::Pending);
        for(unsigned n=0;n<24;++n){testPump();ui->tick(nowMs());tick+=20;}
        assert(ui->outcome()==Outcome::Pending&&channel->live);
        const auto opened=channel->opens;
        ui->cancel();assert(ui->begin(song,nowMs())==Outcome::Ready&&ui->fromCache());
        assert(channel->opens==opened);
        for(unsigned n=0;n<24;++n){testPump();ui->tick(nowMs());tick+=20;}
        assert(ui->outcome()==Outcome::Ready&&ui->fromCache()&&channel->opens==opened);
        assert(!channel->live&&!networkGate().aiActive());channel->stall=false;ui->cancel();
    }
    puts("PASS: real HTTPS session pending Artist/Release cancellation preserves immediate cached Song without reopening TLS");
    ui->trackChanged();tick+=2000;channel->sent.clear();channel->opening=[&]{ui->cancel();};ui->begin(song,nowMs());
    for(int i=0;i<4;++i){testPump();ui->tick(nowMs());tick+=20;}assert(ui->outcome()==Outcome::Canceled&&channel->sent.empty());
    // Repeated in-flight cancellation/timeout releases the socket and shared TLS lease,
    // with a fixed pair of retained runtime/session allocations, not per-song churn.
    const auto allocations=fake::allocations;
    for(unsigned i=0;i<60;++i){
        ui->trackChanged();tick+=2000;channel->stall=true;channel->sent.clear();ui->begin(song,nowMs());
        for(unsigned n=0;n<8;++n){testPump();ui->tick(nowMs());tick+=20;}
        if(ui->outcome()!=Outcome::Pending||!channel->live||!networkGate().aiActive())fprintf(stderr,"cleanup cycle=%u outcome=%u retry_ms=%u live=%d lease=%d\n",i,unsigned(ui->outcome()),ui->retryMs(nowMs()),channel->live,networkGate().aiActive());
        assert(ui->outcome()==Outcome::Pending&&channel->live&&networkGate().aiActive());
        if(i%3==0){tick+=DeadlineMs;testPump();ui->tick(nowMs());assert(ui->outcome()==Outcome::Timeout);}else {ui->cancel();testPump();ui->tick(nowMs());}
        assert(!channel->live&&!networkGate().aiActive()&&fake::liveBytes==bytes&&fake::allocations==allocations);
        assert(networkGate().media());networkGate().release();
    }
    testReset();assert(fake::liveBytes==0);printf("PASS: actual RealSession/API transport/background worker/UI queues with injected fake TLS; ready/cache/original deadline/cancel-during-open before auth; 60 cancel-timeout cleanup cycles with constant allocations and released leases; objects=%zu bytes, no real endpoints contacted\n",bytes);

#endif
}
