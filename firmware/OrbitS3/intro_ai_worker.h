#pragma once
#include "intro_ai_https.h"
namespace orbit_intro_ai {
// Allocate this whole session lazily in PSRAM, never on a task stack.
// Engine + real transport connection point. ONE background owner calls begin,
// step, result and cancel. This class never creates a task or calls LVGL.
// Cross-thread code may ONLY requestCancel(); publish copies through a UI queue
// after the owning worker observes completion. Caller-owned channel outlives it.
class ApiWorkerSession {
public:
    ApiWorkerSession(SecureChannel &channel,ApiSettings settings):channel_(channel),transport_(channel,settings),engine_(transport_,workspace_){}
    Outcome begin(const Snapshot &s,uint64_t absoluteDeadline=0){
        if(engine_.outcome()==Outcome::Pending)return Outcome::Busy;
        canceled_.store(false);transport_.limitDeadline(absoluteDeadline);auto result=engine_.begin(s,channel_.nowMs(),true);
        if(canceled_.load()){transport_.requestCancel();engine_.cancel();return Outcome::Canceled;}return result;
    }
    Outcome step(){
        if(canceled_.load()){engine_.cancel();return Outcome::Canceled;}
        auto result=engine_.poll(engine_.generation(),channel_.nowMs());
        if(canceled_.load()){engine_.discardCurrentResult();return Outcome::Canceled;}return result;
    }
    void requestCancel(){canceled_.store(true);transport_.requestCancel();}
    void cancel(){engine_.cancel();}
    void configurationChanged(){engine_.configurationChanged();}
    void discardCurrentResult(){engine_.discardCurrentResult();}
    void trackChanged(){engine_.trackChanged();} // owner thread only, before delivering a new track
    const Engine &engine()const{return engine_;}
private:
    Workspace workspace_;
    SecureChannel &channel_;
    HttpsTransport transport_;
    Engine engine_;
    std::atomic<bool> canceled_{false};
};
}
