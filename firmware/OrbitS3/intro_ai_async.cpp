#include "intro_ai_async.h"
#include <string.h>
namespace orbit_intro_async {
namespace {
bool sameTrack(const Snapshot &a,const Snapshot &b){return a.durationSeconds==b.durationSeconds&&!strcmp(a.title,b.title)&&!strcmp(a.artist,b.artist)&&!strcmp(a.release,b.release);}
bool negative(Outcome o){return o==Outcome::Ambiguous||o==Outcome::NotFound||o==Outcome::InsufficientEvidence;}
}
bool retryable(Outcome o){switch(o){case Outcome::HttpError:case Outcome::TransportError:case Outcome::Timeout:case Outcome::RateLimited:case Outcome::Offline:case Outcome::ClockUntrusted:case Outcome::ResourceLimited:case Outcome::TlsError:case Outcome::ProtocolError:case Outcome::TooLarge:case Outcome::Malformed:case Outcome::Incomplete:case Outcome::ToolError:return true;default:return false;}}
void UiClient::clear(){memset(static_cast<void*>(cache_),0,sizeof cache_);}
void UiClient::trackChanged(){clear();identified_=false;mail_.epoch.store(mail_.epoch.load(std::memory_order_relaxed)+1,std::memory_order_release);mail_.invalidate();display_.cached=false;if(outcome()==Outcome::Pending)display_.outcome=Outcome::Canceled;}
void UiClient::cancel(){mail_.invalidate();display_.outcome=Outcome::Canceled;display_.cached=false;}
void UiClient::scenario(unsigned value){trackChanged();mail_.scenario.store(value);mail_.scenarioVersion.store(mail_.scenarioVersion.load(std::memory_order_relaxed)+1,std::memory_order_release);mail_.notify();display_.outcome=Outcome::Idle;}
uint32_t UiClient::retryMs(uint64_t now)const{if(now>=display_.retryUntil)return 0;auto n=display_.retryUntil-now;return n>UINT32_MAX?UINT32_MAX:uint32_t(n);}
Outcome UiClient::begin(const Snapshot &s,uint64_t now){
    // Validate/copy into persistent storage; no giant temporary on the UI stack.
    if(!capture(outgoing_.snapshot,s.topic,s.title,s.artist,s.release,s.durationSeconds)){cancel();return display_.outcome=Outcome::InvalidMetadata;}
    if(identified_&&!sameTrack(identity_,s))trackChanged();
    identity_=s;identified_=true;display_.token=mail_.invalidate();display_.epoch=mail_.epoch.load();display_.topic=s.topic;display_.cached=false;display_.retryUntil=0;
    auto &cached=cache_[unsigned(s.topic)];
    if(cached.valid&&now>=cached.at&&(cached.outcome==Outcome::Ready||now-cached.at<MissTtlMs)){
        display_.value=cached.value;display_.outcome=cached.outcome;display_.cached=true;return outcome();
    }
    outgoing_.scenario=mail_.scenario.load();outgoing_.scenarioVersion=mail_.scenarioVersion.load();
    outgoing_.token=display_.token;outgoing_.epoch=display_.epoch;outgoing_.deadline=now+DeadlineMs;started_=now;
    if(!mail_.commands.push(outgoing_)){display_.retryUntil=now+1500;return display_.outcome=Outcome::ResourceLimited;}
    display_.outcome=Outcome::Pending;mail_.notify();return outcome();
}
bool UiClient::tick(uint64_t now){
    bool changed=false,received=false;
    if(outcome()==Outcome::Pending&&(now<started_||now-started_>=DeadlineMs)){
        mail_.invalidate();display_.outcome=Outcome::Timeout;display_.retryUntil=now+FailureCooldownMs;changed=true;
    }
    for(unsigned i=0;i<2&&mail_.results.pop(incoming_);++i){
        received=true;
        if(incoming_.token!=display_.token||incoming_.token!=mail_.token.load()||incoming_.epoch!=mail_.epoch.load()||incoming_.topic!=display_.topic||outcome()!=Outcome::Pending)continue;
        display_=incoming_;changed=true;
        if(outcome()==Outcome::Ready||negative(outcome())){auto &cached=cache_[unsigned(display_.topic)];cached.value=display_.value;cached.at=now;cached.outcome=outcome();cached.valid=true;}
    }
    if(received)mail_.notify();
    return changed;
}
void Worker::synchronizeEpoch(uint32_t epoch){if(epoch_==epoch)return;stop();session_.trackChanged();epoch_=epoch;delivery_=false;}
void Worker::release(){if(leased_){admission_->release();leased_=false;}}
void Worker::stop(){session_.cancel();active_=false;release();}
bool Worker::current()const{return command_.token==mail_.token.load()&&command_.epoch==mail_.epoch.load();}
void Worker::publish(Outcome outcome,uint64_t at){
    reply_.token=command_.token;reply_.epoch=command_.epoch;reply_.topic=command_.snapshot.topic;reply_.outcome=outcome;reply_.cached=session_.engine().fromCache();
    reply_.retryUntil=at+session_.retryMs(at);
    if(outcome==Outcome::ResourceLimited||outcome==Outcome::Timeout)reply_.retryUntil=at+FailureCooldownMs;
    if(outcome==Outcome::Ready&&session_.engine().result())reply_.value=*session_.engine().result();else memset(static_cast<void*>(&reply_.value),0,sizeof reply_.value);
    delivery_=true;active_=false;release();
}
bool Worker::pump(){
    synchronizeEpoch(mail_.epoch.load());
    if(active_&&!current())stop();
    bool have=false;
    for(unsigned i=0;i<2&&mail_.commands.pop(scratch_);++i)if(scratch_.token==mail_.token.load()&&scratch_.epoch==mail_.epoch.load()){command_=scratch_;have=true;}
    if(have){
        stop();delivery_=false;
        // The UI can advance epoch after the initial check and before dequeue.
        // Bind cache lifetime to this exact command, never a later atomic load.
        synchronizeEpoch(command_.epoch);
        if(current()){
            if(scenarioVersion_!=command_.scenarioVersion){session_.scenario(command_.scenario);scenarioVersion_=command_.scenarioVersion;mail_.starts.store(0);}
            if(now()>=command_.deadline)publish(Outcome::Timeout,now());
            else {
                const auto outcome=session_.begin(command_.snapshot,now(),command_.deadline);
                active_=outcome==Outcome::Pending;
                if(active_)mail_.starts.store(mail_.starts.load(std::memory_order_relaxed)+1,std::memory_order_release);
                if(!current()){
                    if((outcome==Outcome::Ready||negative(outcome))&&!session_.engine().fromCache())session_.discardCurrentResult();
                    stop();delivery_=false;
                }else if(now()>=command_.deadline){
                    if(!session_.engine().fromCache())session_.discardCurrentResult();
                    stop();publish(Outcome::Timeout,now());
                }else if(!active_)publish(outcome,now());
            }
        }
    }
    if(active_&&!current())stop();
    if(active_){
        if(now()>=command_.deadline){stop();publish(Outcome::Timeout,now());}
        else if(leased_&&admission_&&admission_->preempted()){stop();publish(Outcome::ResourceLimited,now());}
        else if(!admission_||leased_||(leased_=admission_->acquire())){
            auto outcome=session_.step(now()); // worker-only; possibly bounded TLS slice
            if(!current()){
                if(outcome==Outcome::Ready||negative(outcome))session_.discardCurrentResult();
                stop();delivery_=false;
            }else if(now()>=command_.deadline){session_.discardCurrentResult();stop();publish(Outcome::Timeout,now());}
            else if(outcome!=Outcome::Pending)publish(outcome,now());
        }
    }
    if(delivery_){
        if(!current())delivery_=false;
        else if(now()>=command_.deadline && reply_.outcome!=Outcome::Timeout)delivery_=false;
        else if(mail_.results.push(reply_))delivery_=false;
        else if(now()>=command_.deadline)delivery_=false; // no endless wake loop if UI cannot drain
    }
    return active_||delivery_;
}
}
