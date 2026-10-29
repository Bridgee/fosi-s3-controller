#include "intro_ai_mock.h"
#include <assert.h>
#include <stdio.h>
#include <memory>
#include <string.h>
using namespace orbit_intro_ai;
int main(){
    static_assert(sizeof(MockSession)<59*1024,"Keep local UI session bounded");
    std::unique_ptr<MockSession> m(new MockSession);Snapshot s;
    assert(capture(s,Topic::Artist,"Example song","Example artist","Example EP",240));
    uint32_t now=100;
    for(Topic topic:{Topic::Artist,Topic::Release,Topic::Song}){
        s.topic=topic;auto starts=m->starts();assert(m->begin(s,now)==Outcome::Pending);
        m->tick(now+1499);assert(m->engine().outcome()==Outcome::Pending);
        m->tick(now+1500);assert(m->engine().outcome()==Outcome::Ready);
        auto result=m->engine().result();assert(result&&strstr(result->text,"local sample")&&result->sourceCount==1);
        assert(!strcmp(result->subject,topic==Topic::Artist?s.artist:topic==Topic::Release?s.release:s.title));
        m->cancel();assert(m->begin(s,now+1600)==Outcome::Ready&&m->engine().fromCache()&&m->starts()==starts+1);
        m->cancel();now+=2000;
    }
    const auto completedStarts=m->starts();
    for(Topic topic:{Topic::Artist,Topic::Release,Topic::Song}){s.topic=topic;assert(m->begin(s,now)==Outcome::Ready&&m->engine().fromCache()&&m->starts()==completedStarts);m->cancel();}
    m->trackChanged();now+=2000;assert(m->begin(s,now)==Outcome::Pending);m->tick(now+1500);assert(m->engine().outcome()==Outcome::Ready);m->cancel();now+=2000;
    for(Topic topic:{Topic::Artist,Topic::Release,Topic::Song}){
        m->scenario(MockScenario::FailOnce);s.topic=topic;
        assert(m->begin(s,now)==Outcome::Pending);m->tick(now+1500);assert(m->engine().outcome()==Outcome::HttpError&&m->retryMs()==5000);
        assert(m->begin(s,now+2000)==Outcome::HttpError&&m->starts()==1);
        assert(m->begin(s,now+6500)==Outcome::Pending);m->tick(now+8000);assert(m->engine().outcome()==Outcome::Ready&&m->starts()==2);
        m->cancel();now+=9000;
    }
    m->scenario(MockScenario::RateLimitOnce);assert(m->begin(s,now)==Outcome::Pending);m->tick(now+1500);
    assert(m->engine().outcome()==Outcome::RateLimited&&m->retryMs()==3000);
    assert(m->begin(s,now+4500)==Outcome::Pending);m->tick(now+6000);assert(m->engine().outcome()==Outcome::Ready);
    now+=7000;m->scenario(MockScenario::TimeoutOnce);assert(m->begin(s,now)==Outcome::Pending);m->tick(now+30000);
    assert(m->engine().outcome()==Outcome::Timeout);assert(m->begin(s,now+35000)==Outcome::Pending);m->tick(now+36500);assert(m->engine().outcome()==Outcome::Ready);
    now+=40000;m->scenario(MockScenario::NoMatch);assert(m->begin(s,now)==Outcome::Pending);m->tick(now+1500);
    assert(m->engine().outcome()==Outcome::InsufficientEvidence);m->cancel();assert(m->begin(s,now+1600)==Outcome::InsufficientEvidence&&m->engine().fromCache());
    now+=2000;m->scenario(MockScenario::Success);s.topic=Topic::Release;s.release[0]=0;assert(m->begin(s,now)==Outcome::Pending);m->tick(now+1500);assert(m->engine().outcome()==Outcome::Ambiguous);
    now+=2000;m->scenario(MockScenario::Success);strcpy(s.release,"Example EP");assert(m->begin(s,now)==Outcome::Pending);m->cancel();m->tick(now+2000);assert(m->engine().outcome()==Outcome::Canceled&&!m->engine().result());
    strcpy(s.title,"New song");assert(m->begin(s,now+2100)==Outcome::Pending);m->tick(now+3600);assert(m->engine().outcome()==Outcome::Ready);
    // UI LVGL clock extension, including a uint32 tick wrap while loading.
    m.reset(new MockSession);s.topic=Topic::Song;assert(m->begin(s,0xfffffff0u)==Outcome::Pending);m->tick(uint32_t(0xfffffff0u+1500u));assert(m->engine().outcome()==Outcome::Ready);
    printf("PASS: local MockSession uses actual request/parser/cache Engine for all topics, first-failure retry, 429, deadline, no-match, cancel and clock wrap; %zu bytes; no network/device code\n",sizeof(MockSession));
}
