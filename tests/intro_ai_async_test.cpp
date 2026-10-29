#include <initializer_list>
#include <cstdlib>
#include "intro_ai_async.h"
#include "intro_ai_mock.h"
#include "intro_ai_gate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <type_traits>
using namespace orbit_intro_async;
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
struct ClockState {std::atomic<uint64_t> at{100};static uint64_t read(void *p){return static_cast<ClockState*>(p)->at.load();}};
struct Local:Session {
    MockSession mock;std::atomic<bool> canceled{false};std::function<void()> boundary,onCancel,onBegin;uint64_t lastDeadline=0;
    Outcome begin(const Snapshot &s,uint64_t n,uint64_t deadline)override{lastDeadline=deadline;canceled.store(false);if(onBegin){auto f=onBegin;onBegin=nullptr;f();}return mock.begin(s,uint32_t(n));}
    Outcome step(uint64_t n)override{if(canceled.load()){mock.cancel();return Outcome::Canceled;}mock.tick(uint32_t(n));if(boundary){auto f=boundary;boundary=nullptr;f();}return mock.engine().outcome();}
    void requestCancel()override{canceled.store(true);}
    void cancel()override{if(onCancel){auto f=onCancel;onCancel=nullptr;f();}mock.cancel();}
    void trackChanged()override{mock.trackChanged();}
    void discardCurrentResult()override{mock.discardCurrentResult();}
    void scenario(unsigned n)override{mock.scenario(static_cast<MockScenario>(n));}
    const Engine &engine()const override{return mock.engine();}
    uint32_t retryMs(uint64_t)const override{return mock.retryMs();}
};
struct Gate:Admission {bool allowed=true,preempt=false,held=false;unsigned acquired=0,released=0;
    bool acquire()override{if(!allowed)return false;held=true;++acquired;return true;}
    bool preempted()const override{return preempt;}
    void release()override{assert(held);held=false;++released;preempt=false;}
};
struct Rig {
    ClockState clock;Local session;Mailbox mail;UiClient ui;Gate gate;Worker worker;Snapshot song;
    Rig():mail(session),ui(mail),worker(mail,ClockState::read,&clock,&gate){assert(capture(song,Topic::Song,"First song","Artist","Release",250));}
    uint64_t now(){return clock.at.load();}
    void pump(uint64_t advance=0,bool receive=true){clock.at.fetch_add(advance);worker.pump();if(receive)ui.tick(now());}
    void ready(Topic t){song.topic=t;CHECK(ui.begin(song,now())==Outcome::Pending);pump();pump(1499);CHECK(ui.outcome()==Outcome::Pending);pump(1);CHECK(ui.outcome()==Outcome::Ready&&ui.result());}
};
int main(){
    static_assert(!std::is_copy_constructible<UiClient>::value&&!std::is_copy_constructible<Worker>::value,"one UI producer / worker owner");
    static_assert(sizeof(Mailbox)<10*1024,"fixed queue storage bound");
    static_assert(sizeof(UiClient)<21*1024,"UI cache/view storage bound");
    std::unique_ptr<Rig> r(new Rig);
    for(Topic t:{Topic::Artist,Topic::Release,Topic::Song}){r->ready(t);r->ui.cancel();r->pump();r->pump(2000);}
    CHECK(r->mail.starts.load()==3);
    for(Topic t:{Topic::Artist,Topic::Release,Topic::Song}){r->song.topic=t;CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready&&r->ui.fromCache());r->pump();CHECK(r->mail.starts.load()==3);r->ui.cancel();}
    r->ui.trackChanged();r->pump(2000);CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);r->pump();r->pump(1500);CHECK(r->mail.starts.load()==4);
    // Cancel before the worker takes a command starts no request at all.
    r.reset(new Rig);CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);r->ui.cancel();r->pump(2000);CHECK(r->mail.starts.load()==0&&r->ui.outcome()==Outcome::Canceled);
    r->ready(Topic::Artist);r->ui.cancel();r->pump(2000);r->song.topic=Topic::Song;r->ui.begin(r->song,r->now());r->pump();r->ui.cancel();r->pump(2000);r->song.topic=Topic::Artist;CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready&&r->ui.fromCache());
    // Track change cancels in flight. A->B->A cannot revive the first cache.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump();r->ui.trackChanged();strcpy(r->song.title,"Second song");r->pump(2000);r->ready(Topic::Song);CHECK(!strcmp(r->ui.result()->subject,"Second song"));
    r->ui.trackChanged();strcpy(r->song.title,"First song");r->pump(2000);CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);
    // Completed message queued before Back is filtered; worker's cache avoids a second call.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump(0,false);r->pump(1500,false);r->ui.cancel();r->ui.tick(r->now());CHECK(r->ui.outcome()==Outcome::Canceled);
    CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);r->pump();CHECK(r->ui.outcome()==Outcome::Ready&&r->ui.fromCache()&&r->mail.starts.load()==1);
    // A canceled parse result cannot refill cache after the generation changes.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump();r->session.boundary=[&]{r->ui.trackChanged();};r->pump(1500);CHECK(r->ui.outcome()==Outcome::Canceled&&!r->ui.result());r->pump(2000);CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);r->pump();CHECK(r->mail.starts.load()==2);
    // Queue saturation is bounded; obsolete commands never start.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->ui.cancel();r->ui.begin(r->song,r->now());r->ui.cancel();CHECK(r->ui.begin(r->song,r->now())==Outcome::ResourceLimited);r->pump();CHECK(r->mail.starts.load()==0);
    // Full result queue retains one bounded pending delivery, never resubmits.
    r.reset(new Rig);Message dummy;CHECK(r->mail.results.push(dummy)&&r->mail.results.push(dummy)&&!r->mail.results.push(dummy));r->ui.begin(r->song,r->now());r->pump(0,false);r->pump(1500,false);r->pump(100,false);CHECK(r->mail.starts.load()==1);r->ui.tick(r->now());r->pump();CHECK(r->ui.outcome()==Outcome::Ready&&r->mail.starts.load()==1);
    // A full, unconsumed result queue must not keep the worker waking forever.
    r.reset(new Rig);CHECK(r->mail.results.push(dummy)&&r->mail.results.push(dummy));r->ui.begin(r->song,r->now());r->pump(0,false);r->pump(1500,false);
    r->clock.at.fetch_add(DeadlineMs);CHECK(!r->worker.pump()&&!r->gate.held&&r->mail.starts.load()==1);r->ui.tick(r->now());CHECK(r->ui.outcome()==Outcome::Timeout);
    // A timely worker result can sit unread until the UI's absolute deadline.
    // Expiry must win over dequeue, and must not fill the UI cache with late data.
    for(auto scenario:{MockScenario::Success,MockScenario::NoMatch}){
        r.reset(new Rig);r->ui.scenario(unsigned(scenario));const auto requested=r->now();r->ui.begin(r->song,requested);
        r->pump(0,false);r->pump(1500,false);CHECK(r->ui.outcome()==Outcome::Pending);
        r->clock.at.store(requested+DeadlineMs);CHECK(r->ui.tick(r->now()) && r->ui.outcome()==Outcome::Timeout && !r->ui.result());
        r->ui.cancel();CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending && !r->ui.fromCache());
    }
    r.reset(new Rig);const auto justInTime=r->now();r->ui.begin(r->song,justInTime);r->pump(0,false);r->pump(1500,false);
    r->clock.at.store(justInTime+DeadlineMs-1);CHECK(r->ui.tick(r->now()) && r->ui.outcome()==Outcome::Ready && r->ui.result());
    r->ui.cancel();CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready && r->ui.fromCache());
    // Admission waiting and running requests share the original UI deadline.
    r.reset(new Rig);r->gate.allowed=false;r->ui.begin(r->song,r->now());r->pump();r->pump(DeadlineMs);CHECK(r->ui.outcome()==Outcome::Timeout&&!r->gate.held);CHECK(r->ui.retryMs(r->now())==FailureCooldownMs);
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump();CHECK(r->gate.held);r->gate.preempt=true;r->pump(100);CHECK(r->ui.outcome()==Outcome::ResourceLimited&&!r->gate.held);CHECK(r->gate.acquired==r->gate.released);
    // Already-validated synthetic worker messages retain all per-topic references
    // and paragraph masks across Back/reopen. Track change releases the cache,
    // while a currently open ready reader still owns its captured full value.
    r.reset(new Rig);
    for(unsigned topic=0;topic<3;++topic){
        r->song.topic=Topic(topic);r->ui.begin(r->song,r->now());r->pump(0,false);r->pump(1500,false);
        Message message;CHECK(r->mail.results.pop(message) && message.outcome==Outcome::Ready);
        snprintf(message.value.sources[0].url,sizeof message.value.sources[0].url,"https://example.org/topic/%u",topic);
        snprintf(message.value.sources[0].title,sizeof message.value.sources[0].title,"Synthetic topic %u reference",topic);
        CHECK(r->mail.results.push(message));r->ui.tick(r->now());CHECK(r->ui.result() && r->ui.result()->sourceCount==1);
        r->ui.cancel();r->pump(100);
    }
    const auto referenceStarts=r->mail.starts.load();
    for(unsigned topic=0;topic<3;++topic){r->song.topic=Topic(topic);CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready && r->ui.fromCache());
        const auto *value=r->ui.result();char expected[64];snprintf(expected,sizeof expected,"https://example.org/topic/%u",topic);
        CHECK(value && !strcmp(value->sources[0].url,expected) && value->paragraphSources[0]==1 && value->paragraphSources[1]==1);
        CHECK(r->mail.starts.load()==referenceStarts);
    }
    r->ui.trackChanged();CHECK(r->ui.result() && !strcmp(r->ui.result()->sources[0].url,"https://example.org/topic/2"));
    CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending && !r->ui.fromCache());
    // Explicit retry only. A cached success bypasses both queues and cooldown.
    r.reset(new Rig);r->ui.scenario(unsigned(MockScenario::FailOnce));r->ui.begin(r->song,r->now());r->pump();r->pump(1500);CHECK(r->ui.outcome()==Outcome::HttpError&&r->ui.retryMs(r->now())==5000);for(int i=0;i<20;++i)r->pump(1000);CHECK(r->mail.starts.load()==1);r->ready(Topic::Song);CHECK(r->mail.starts.load()==2);r->ui.cancel();CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready);
    // No lifetime expiry of completed text while the same track remains selected.
    r->ui.cancel();r->clock.at.fetch_add(48ULL*3600000);CHECK(r->ui.begin(r->song,r->now())==Outcome::Ready&&r->ui.fromCache());
    // Reset during an old completion binds scenario/version to the new command.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump();r->session.boundary=[&]{r->ui.scenario(unsigned(MockScenario::FailOnce));r->ui.begin(r->song,r->now());};
    r->pump(1500);r->pump();r->pump(1500);CHECK(r->ui.outcome()==Outcome::HttpError&&r->ui.retryMs(r->now())==5000);
    // UI's total deadline also cancels a command that was never dequeued.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->clock.at.fetch_add(DeadlineMs);CHECK(r->ui.tick(r->now())&&r->ui.outcome()==Outcome::Timeout);r->pump();CHECK(r->mail.starts.load()==0);
    // Epoch changes after the pump's first check but before it takes a new command.
    // Returning to A's exact metadata must not revive A's old worker cache.
    r.reset(new Rig);r->ready(Topic::Song);r->ui.cancel();r->pump(2000);
    r->song.topic=Topic::Artist;r->ui.begin(r->song,r->now());r->pump();r->ui.cancel();
    r->session.onCancel=[&]{r->ui.trackChanged();r->ui.trackChanged();r->song.topic=Topic::Song;r->ui.begin(r->song,r->now());};
    r->pump();CHECK(r->ui.outcome()!=Outcome::Ready&&!r->ui.fromCache()&&!r->ui.result());
    r->pump(2000);CHECK(r->ui.begin(r->song,r->now())==Outcome::Pending);r->pump();CHECK(r->ui.outcome()==Outcome::Pending);
    // Command waiting consumes the same deadline passed to the transport.
    r.reset(new Rig);const auto originalDeadline=r->now()+DeadlineMs;r->ui.begin(r->song,r->now());r->pump(12000);CHECK(r->session.lastDeadline==originalDeadline);
    // Even an immediate cache/error begin must not publish after the deadline.
    r.reset(new Rig);r->session.onBegin=[&]{r->clock.at.fetch_add(DeadlineMs);};r->ui.begin(r->song,r->now());r->pump();CHECK(r->ui.outcome()==Outcome::Timeout&&!r->gate.held);
    // A worker-only cache hit can cross the deadline during begin itself.
    // Reject late delivery but retain the previously completed cache entry.
    r.reset(new Rig);r->ui.begin(r->song,r->now());r->pump(0,false);r->pump(1500,false);r->ui.cancel();r->ui.tick(r->now());
    r->session.onBegin=[&]{r->clock.at.fetch_add(DeadlineMs);};r->ui.begin(r->song,r->now());r->pump();CHECK(r->ui.outcome()==Outcome::Timeout&&!r->ui.result()&&r->mail.starts.load()==1);
    r->pump(FailureCooldownMs);r->ui.begin(r->song,r->now());r->pump();CHECK(r->ui.outcome()==Outcome::Ready&&r->ui.fromCache()&&r->mail.starts.load()==1);
    // Destroying a stopped worker cancels its session and returns the shared lease.
    {ClockState clock;Local session;Mailbox mail(session);UiClient ui(mail);Gate shared;Snapshot song;CHECK(capture(song,Topic::Song,"Song","Artist","Release",100));
     std::unique_ptr<Worker> owner(new Worker(mail,ClockState::read,&clock,&shared));ui.begin(song,clock.at.load());owner->pump();CHECK(shared.held);owner.reset();CHECK(!shared.held&&shared.acquired==shared.released&&session.engine().outcome()==Outcome::Canceled);}
    NetworkGate gate;CHECK(gate.media()&&!gate.ai());gate.release();CHECK(gate.ai()&&!gate.media());gate.preempt();CHECK(gate.preempted());gate.release();CHECK(gate.ai()&&!gate.preempted());gate.release();
    // Actual concurrent SPSC publication: no lost/torn structs or unbounded growth.
    Queue<Command,2> queue;std::atomic<bool> done{false};std::thread producer([&]{Command c;for(unsigned i=1;i<=20000;++i){c.token=i;c.snapshot.durationSeconds=i;snprintf(c.snapshot.title,sizeof c.snapshot.title,"%u",i);while(!queue.push(c))std::this_thread::yield();}done.store(true);});
    Command c;unsigned received=0;while(!done.load()||received<20000){if(queue.pop(c)){++received;assert(c.token==received&&c.snapshot.durationSeconds==received&&unsigned(std::atoi(c.snapshot.title))==received);}else std::this_thread::yield();}producer.join();CHECK(received==20000);
    // Actual UI/worker threads repeatedly replace/cancel requests during parsing.
    r.reset(new Rig);done.store(false);std::thread background([&]{while(!done.load()){r->worker.pump();std::this_thread::yield();}r->worker.shutdown();});
    for(unsigned i=0;i<1200;++i){r->song.topic=Topic(i%3);if(i%11==0){r->ui.trackChanged();snprintf(r->song.title,sizeof r->song.title,"Track %u",i);}r->ui.begin(r->song,r->now());if(i%3==0)r->ui.cancel();r->clock.at.fetch_add(20);r->ui.tick(r->now());if(auto *v=r->ui.result())CHECK(v->text[0]&&v->sourceCount>0);std::this_thread::yield();}
    r->ui.cancel();done.store(true);background.join();CHECK(!r->gate.held); // no concurrent ownership after join
    printf("PASS: %u async assertions; real Engine/mock transport; bounded queues, background ownership, load/cancel/track/cache/late delivery/backpressure/timeout/preemption, 20000 concurrent queue messages and 1200 threaded UI replacements; mailbox=%zu UI=%zu worker=%zu bytes; no network/devices\n",checks,sizeof(Mailbox),sizeof(UiClient),sizeof(Worker));
}
