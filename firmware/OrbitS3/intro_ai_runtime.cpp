#include "intro_ai_runtime.h"
#if ORBIT_INTRO_ASYNC
#include "intro_ai_esp32.h"
#include "intro_ai_worker.h"
#include "intro_ai_gate.h"
#if ORBIT_INTRO_MOCK
#include "intro_ai_mock.h"
#endif
#include <new>
#if defined(ORBIT_AI_RUNTIME_HOST_TEST)
#if defined(ARDUINO)
#error "Synthetic task platform must never compile for hardware"
#endif
#include "intro_async_platform.h"
#else
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace orbit_intro_platform {
using Task=TaskHandle_t;
uint64_t clock(){return uint64_t(esp_timer_get_time())/1000;}
void *allocate(size_t bytes){return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
void dispose(void *p){free(p);}
bool admitted(bool real){return heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=(real?224u:64u)*1024 && heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=(real?96u:16u)*1024;}
bool start(void(*entry)(void*),void *arg,unsigned bytes,Task &task){return xTaskCreatePinnedToCore(entry,"intro-ai",bytes,arg,1,&task,0)==pdPASS;}
void notify(Task task){if(task)xTaskNotifyGive(task);}
void wait(bool active){ulTaskNotifyTake(pdTRUE,active?pdMS_TO_TICKS(20):portMAX_DELAY);}
}
#endif
namespace orbit_intro_runtime {
using namespace orbit_intro_async;
namespace {
#if !ORBIT_INTRO_MOCK
class RealSession final:public Session {
    ApiWorkerSession session_;
public:
    RealSession(SecureChannel &channel,ApiSettings settings):session_(channel,settings){}
    Outcome begin(const Snapshot &s,uint64_t,uint64_t deadline)override{return session_.begin(s,deadline);}
    Outcome step(uint64_t)override{return session_.step();}
    void requestCancel()override{session_.requestCancel();}
    void cancel()override{session_.cancel();}
    void trackChanged()override{session_.trackChanged();}
    void discardCurrentResult()override{session_.discardCurrentResult();}
    const Engine &engine()const override{return session_.engine();}
};
#endif
#if ORBIT_INTRO_MOCK
class LocalSession final:public Session {
    MockSession session_;std::atomic<bool> canceled_{false};
public:
    Outcome begin(const Snapshot &s,uint64_t now,uint64_t)override{canceled_.store(false);return session_.begin(s,uint32_t(now));}
    Outcome step(uint64_t now)override{if(canceled_.load()){session_.cancel();return Outcome::Canceled;}session_.tick(uint32_t(now));return session_.engine().outcome();}
    void requestCancel()override{canceled_.store(true);}
    void cancel()override{session_.cancel();}
    void trackChanged()override{session_.trackChanged();}
    void discardCurrentResult()override{session_.discardCurrentResult();}
    void scenario(unsigned s)override{if(s<=unsigned(MockScenario::NoMatch))session_.scenario(static_cast<MockScenario>(s));}
    const Engine &engine()const override{return session_.engine();}
    uint32_t retryMs(uint64_t)const override{return session_.retryMs();}
};
#endif
class GateAdmission:public Admission {
public:
    bool acquire()override{
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK
        return networkGate().ai();
#else
        return true;
#endif
    }
    bool preempted()const override{
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK
        return networkGate().preempted();
#else
        return false;
#endif
    }
    void release()override{
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK
        networkGate().release();
#endif
    }
};
uint64_t clock(void*){return orbit_intro_platform::clock();}
struct Runtime {
    Session &session;SecureChannel *channel;bool real;
    GateAdmission admission;Mailbox mail;UiClient ui;Worker worker;
    orbit_intro_platform::Task task=nullptr;
    Runtime(Session &s,SecureChannel *c,bool r):session(s),channel(c),real(r),mail(s),ui(mail),worker(mail,clock,nullptr,r?&admission:nullptr){}
};
Runtime *running=nullptr;Outcome startup=Outcome::Disabled;
void wake(void *context){auto *r=static_cast<Runtime*>(context);orbit_intro_platform::notify(r->task);}
void entry(void *context){auto *r=static_cast<Runtime*>(context);for(;;){bool active=r->worker.pump();orbit_intro_platform::wait(active);}}
void destroyChannel(SecureChannel *channel){
#if !ORBIT_INTRO_MOCK
    destroyEsp32Channel(channel);
#else
    (void)channel;
#endif
}
void destroySession(Session *session){
#if ORBIT_INTRO_MOCK
    static_cast<LocalSession*>(session)->~LocalSession();
#else
    static_cast<RealSession*>(session)->~RealSession();
#endif
    orbit_intro_platform::dispose(session);
}
void dispose(Runtime *r){
    if(!r)return;Session *session=&r->session;auto *channel=r->channel;
    r->~Runtime();orbit_intro_platform::dispose(r);destroySession(session);destroyChannel(channel);
}
// Only client() may call this on a deliberate UI selection, never boot/reconnect.
Runtime *start(bool real,ApiSettings settings){
    if(!orbit_intro_platform::admitted(real)){startup=Outcome::ResourceLimited;return nullptr;}
    SecureChannel *channel=nullptr;Session *session=nullptr;
#if ORBIT_INTRO_MOCK
    (void)settings;
    if(real){startup=Outcome::Disabled;return nullptr;}
    {void *memory=orbit_intro_platform::allocate(sizeof(LocalSession));if(memory)session=new(memory) LocalSession;}
#else
    channel=createEsp32Channel();if(!channel){startup=Outcome::ResourceLimited;return nullptr;}
    {void *memory=orbit_intro_platform::allocate(sizeof(RealSession));if(memory)session=new(memory) RealSession(*channel,settings);}
#endif
    if(!session){destroyChannel(channel);startup=Outcome::ResourceLimited;return nullptr;}
    void *memory=orbit_intro_platform::allocate(sizeof(Runtime));
    if(!memory){destroySession(session);destroyChannel(channel);startup=Outcome::ResourceLimited;return nullptr;}
    auto *r=new(memory) Runtime(*session,channel,real);r->mail.wake=wake;r->mail.wakeContext=r;
    if(!orbit_intro_platform::start(entry,r,real?16384:8192,r->task)){dispose(r);startup=Outcome::ResourceLimited;return nullptr;}
    startup=Outcome::Idle;return r;
}
}
uint64_t nowMs(){return orbit_intro_platform::clock();}
Outcome startupOutcome(){return startup;}
UiClient *client(){
    if(running)return &running->ui;
#if ORBIT_INTRO_MOCK
    running=start(false,ApiSettings{});
#elif ORBIT_AI_ENABLED && ORBIT_LIVE && !defined(ORBIT_BUILD_CHECK) && !ORBIT_AI_TRANSPORT_CHECK
    const auto settings=configuredApiSettings();
    if(validateSettings(settings)!=SettingsStatus::Ready){startup=Outcome::ConfigError;return nullptr;}
    running=start(true,settings);
#else
    // Verification cannot start a task or include private credentials, even if
    // an enable macro was supplied. Never inspect CA/key here in an off build.
    startup=Outcome::Disabled;
#endif
    return running?&running->ui:nullptr;
}
#if defined(ORBIT_AI_RUNTIME_HOST_TEST)
void testPump(){if(running)running->worker.pump();}
void testReset(){dispose(running);running=nullptr;startup=Outcome::Disabled;}
#if !ORBIT_INTRO_MOCK
UiClient *testRealClient(ApiSettings settings){if(!running)running=start(true,settings);return running?&running->ui:nullptr;}
#endif
#endif
#if ORBIT_AI_TRANSPORT_CHECK
// Link-only address probe; never starts a task or executes provider operations.
extern "C" const unsigned orbit_intro_runtime_sizes[]={sizeof(Runtime),sizeof(RealSession),sizeof(Mailbox),sizeof(UiClient),sizeof(Worker),sizeof(ApiWorkerSession)};
extern "C" uintptr_t orbit_ai_runtime_link_probe(){return reinterpret_cast<uintptr_t>(&entry)^reinterpret_cast<uintptr_t>(&start)^reinterpret_cast<uintptr_t>(orbit_intro_runtime_sizes);}
#endif
}
#endif
