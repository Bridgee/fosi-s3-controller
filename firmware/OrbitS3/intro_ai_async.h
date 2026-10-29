#pragma once
#include "intro_ai.h"
#include <atomic>
namespace orbit_intro_async {
using namespace orbit_intro_ai;
// Bounded SPSC queues: one UI producer and one worker, never an overwrite race.
// Storage belongs in PSRAM. No allocator, spin/wait loop or locks in UI calls.
template<class T,unsigned N> class Queue {
    static_assert(N && !(N&(N-1)),"Queue depth must be a power of two");
    T slots_[N];std::atomic<unsigned> write_{0},read_{0};
public:
    bool push(const T &v){auto w=write_.load(std::memory_order_relaxed);if(w-read_.load(std::memory_order_acquire)>=N)return false;slots_[w%N]=v;write_.store(w+1,std::memory_order_release);return true;}
    bool pop(T &v){auto n=read_.load(std::memory_order_relaxed);if(n==write_.load(std::memory_order_acquire))return false;v=slots_[n%N];read_.store(n+1,std::memory_order_release);return true;}
};
struct Command {Snapshot snapshot;uint32_t token=0,epoch=0,scenarioVersion=0;unsigned scenario=0;uint64_t deadline=0;};
struct Message {Introduction value;uint64_t retryUntil=0;uint32_t token=0,epoch=0;Topic topic=Topic::Song;Outcome outcome=Outcome::Idle;bool cached=false;};
class Session {
public:
    virtual ~Session(){}
    virtual Outcome begin(const Snapshot &,uint64_t now,uint64_t absoluteDeadline)=0;
    virtual Outcome step(uint64_t)=0;
    virtual void requestCancel()=0; // ONLY thread-safe Session operation
    virtual void cancel()=0;
    virtual void trackChanged()=0;
    virtual void discardCurrentResult()=0;
    virtual void scenario(unsigned){} // synthetic adapter only
    virtual const Engine &engine()const=0;
    virtual uint32_t retryMs(uint64_t now)const{return engine().retryInMs(now);}
};
class Admission {
public:
    virtual ~Admission(){}
    virtual bool acquire()=0;
    virtual bool preempted()const=0;
    virtual void release()=0;
};
using Clock=uint64_t(*)(void *);
using Wake=void(*)(void *);
struct Mailbox {
    Queue<Command,2> commands;Queue<Message,2> results;
    std::atomic<uint32_t> token{0},epoch{1},scenarioVersion{0};
    std::atomic<unsigned> scenario{0},starts{0};
    Session &session;Wake wake=nullptr;void *wakeContext=nullptr;
    explicit Mailbox(Session &s):session(s){}
    void notify(){if(wake)wake(wakeContext);}
    // Only the UI writes token: plain atomic load/store, never PSRAM CAS.
    uint32_t invalidate(){auto t=token.load(std::memory_order_relaxed)+1;token.store(t,std::memory_order_release);session.requestCancel();notify();return t;}
};
bool retryable(Outcome);
// UI-owned view/cache. Every operation is nonblocking and copies bounded data.
// Engine, transport, parse and network operations exist ONLY on Worker.
class UiClient {
    struct Cached {Introduction value;uint64_t at=0;Outcome outcome=Outcome::Idle;bool valid=false;};
    Mailbox &mail_;Snapshot identity_;Cached cache_[3];
    Command outgoing_;Message incoming_,display_;
    bool identified_=false;uint64_t started_=0;
    void clear();
public:
    explicit UiClient(Mailbox &m):mail_(m){}
    UiClient(const UiClient&)=delete;
    UiClient &operator=(const UiClient&)=delete;
    Outcome begin(const Snapshot &,uint64_t now);
    bool tick(uint64_t now);
    void cancel();
    void trackChanged();
    void scenario(unsigned);
    Outcome outcome()const{return display_.outcome;}
    const Introduction *result()const{return outcome()==Outcome::Ready?&display_.value:nullptr;}
    bool fromCache()const{return display_.cached;}
    uint32_t retryMs(uint64_t now)const;
    unsigned starts()const{return mail_.starts.load();}
};
// One owner. pump() performs at most one Engine/network step, with two-command
// draining and two-slot delivery. A blocked SDK slice never touches UI objects.
class Worker {
    Mailbox &mail_;Session &session_;Clock clock_;void *clockContext_;Admission *admission_;
    Command command_,scratch_;Message reply_;
    uint32_t epoch_=0,scenarioVersion_=~0u;
    bool active_=false,delivery_=false,leased_=false;
    uint64_t now()const{return clock_(clockContext_);}
    void synchronizeEpoch(uint32_t);
    void release();
    void stop();
    void publish(Outcome,uint64_t);
    bool current()const;
public:
    Worker(Mailbox &m,Clock c,void *ctx=nullptr,Admission *a=nullptr):mail_(m),session_(m.session),clock_(c),clockContext_(ctx),admission_(a){}
    ~Worker(){shutdown();} // owner must have stopped/joined any task first
    Worker(const Worker&)=delete;
    Worker &operator=(const Worker&)=delete;
    bool pump();
    void shutdown(){stop();delivery_=false;}
};
}
