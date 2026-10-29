#pragma once
#include "config.h"
#include <atomic>
namespace orbit_intro_async {
// Serializes the new AI TLS session with existing media TLS work. Control/phone
// work requests preemption; its queues are kept intact while the AI slice ends.
class NetworkGate {
    std::atomic<unsigned> owner_{0};std::atomic<bool> preempt_{false};
public:
    bool media(){unsigned zero=0;return owner_.compare_exchange_strong(zero,1);}
    bool ai(){preempt_.store(false);unsigned zero=0;return owner_.compare_exchange_strong(zero,2);}
    void release(){owner_.store(0);}
    void preempt(){if(owner_.load()==2)preempt_.store(true);}
    bool preempted()const{return preempt_.load();}
    bool aiActive()const{return owner_.load()==2;}
};
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK
NetworkGate &networkGate();
class MediaLease {bool held_=false;public:explicit MediaLease(bool needed=true){held_=!needed||networkGate().media();needed_=needed;}~MediaLease(){if(held_&&needed_)networkGate().release();}explicit operator bool()const{return held_;}private:bool needed_=true;};
inline void preemptAi(){networkGate().preempt();}
#else
class MediaLease {public:explicit MediaLease(bool=true){}explicit operator bool()const{return true;}};
inline void preemptAi(){}
#endif
}
