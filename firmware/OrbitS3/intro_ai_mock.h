#pragma once
#include "intro_ai.h"
namespace orbit_intro_ai {
enum class MockScenario:uint8_t {Success,FailOnce,RateLimitOnce,TimeoutOnce,NoMatch};
// Local-only transport: no sockets, keys, Wi-Fi, S3 or device actions.
class MockTransport:public Transport {
public:
    explicit MockTransport(Workspace &w):workspace_(w){}
    void prepare(const Snapshot &,uint64_t now);
    void now(uint64_t value){now_=value;}
    void scenario(MockScenario value);
    unsigned starts()const{return starts_;}
    bool start(const char *,const char *,size_t,uint32_t,uint32_t)override;
    Poll poll(char *,size_t,HttpResult &)override;
    void cancel()override{active_=false;}
private:
    Workspace &workspace_;
    Snapshot snapshot_,failureContext_;
    MockScenario scenario_=MockScenario::Success;
    uint64_t now_=0,started_=0;
    uint32_t generation_=0;
    unsigned starts_=0;
    uint8_t failedTopics_=0;
    bool active_=false,fail_=false;
};
// Single-owner mock adapter. Async runtime owns it on the background worker;
// standalone tests may pump it directly. Engine/request/parser/cache are real.
class MockSession {
public:
    MockSession():transport_(workspace_),engine_(transport_,workspace_){}
    Outcome begin(const Snapshot &,uint32_t tick);
    bool tick(uint32_t tick);
    void cancel(){engine_.cancel();}
    void scenario(MockScenario s){engine_.configurationChanged();transport_.scenario(s);}
    void clearCache(){engine_.clearCache();}
    void discardCurrentResult(){engine_.discardCurrentResult();}
    void trackChanged(){engine_.trackChanged();}
    const Engine &engine()const{return engine_;}
    uint32_t retryMs()const{return engine_.retryInMs(now_);}
    unsigned starts()const{return transport_.starts();}
private:
    Workspace workspace_;
    MockTransport transport_;
    Engine engine_;
    uint64_t now_=0;
    uint32_t lastTick_=0;
    bool initialized_=false;
    void clock(uint32_t tick);
};
}
