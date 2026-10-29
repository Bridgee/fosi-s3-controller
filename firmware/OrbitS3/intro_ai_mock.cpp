#include "config.h"
#if ORBIT_INTRO_MOCK
#include "intro_ai_mock.h"
#include "intro_ai_mock_data.h"
#include <ArduinoJson.h>
#include <string.h>
namespace orbit_intro_ai {
namespace {
struct Arena {
    void *data;size_t capacity;
    void *allocate(size_t size){return size<=capacity?data:nullptr;}
    void deallocate(void*){}
    void *reallocate(void*,size_t){return nullptr;}
};
using Document=BasicJsonDocument<Arena>;
bool sameContext(const Snapshot &a,const Snapshot &b){return a.durationSeconds==b.durationSeconds&&!strcmp(a.title,b.title)&&!strcmp(a.artist,b.artist)&&!strcmp(a.release,b.release);}
}
void MockTransport::prepare(const Snapshot &s,uint64_t now){
    if(!sameContext(failureContext_,s)){failureContext_=s;failedTopics_=0;}
    snapshot_=s;now_=now;
}
void MockTransport::scenario(MockScenario s){scenario_=s;failedTopics_=0;starts_=0;active_=false;}
bool MockTransport::start(const char *endpoint,const char *request,size_t size,uint32_t generation,uint32_t){
    if(strcmp(endpoint,Endpoint)||!request||!size||active_)return false;
    generation_=generation;started_=now_;active_=true;++starts_;
    const unsigned bit=1u<<unsigned(snapshot_.topic);
    fail_=scenario_!=MockScenario::Success&&scenario_!=MockScenario::NoMatch&&!(failedTopics_&bit);
    if(fail_)failedTopics_|=uint8_t(bit);
    return true;
}
Poll MockTransport::poll(char *body,size_t capacity,HttpResult &reply){
    if(!active_)return Poll::Pending;
    if(fail_&&scenario_==MockScenario::TimeoutOnce)return Poll::Pending;
    if(now_-started_<MockDelayMs)return Poll::Pending;
    reply.generation=generation_;
    if(fail_){reply.status=scenario_==MockScenario::RateLimitOnce?429:503;reply.retryAfterSeconds=reply.status==429?3:0;return Poll::Complete;}
    Document doc(JsonPoolBytes,Arena{workspace_.jsonPool,JsonPoolBytes});
    const bool unknownRelease=snapshot_.topic==Topic::Release&&!present(snapshot_.release);
    const bool missing=scenario_==MockScenario::NoMatch;
    doc["status"]=unknownRelease?"ambiguous":missing?"insufficient_evidence":"ready";
    doc["subject"]=unknownRelease||missing?"":snapshot_.topic==Topic::Artist?static_cast<const char*>(snapshot_.artist):snapshot_.topic==Topic::Release?static_cast<const char*>(snapshot_.release):static_cast<const char*>(snapshot_.title);
    doc["release_type"]="unknown";auto paragraphs=doc["paragraphs"].to<JsonArray>();
    if(!unknownRelease&&!missing)for(unsigned i=0;i<2;++i){auto p=paragraphs.createNestedObject();p["text"]=MockParagraphs[unsigned(snapshot_.topic)][i];p["source_urls"].add("https://example.org/local-introduction");}
    if(doc.overflowed()||measureJson(doc)>=4096){reply.tooLarge=true;reply.status=200;return Poll::Complete;}
    size_t bytes=serializeJson(doc,workspace_.request,4096);workspace_.request[bytes]=0;doc.clear();
    doc["id"]="resp_local_sample";doc["status"]="completed";doc["error"]=nullptr;doc["incomplete_details"]=nullptr;
    auto output=doc["output"].to<JsonArray>();auto search=output.createNestedObject();search["type"]="web_search_call";search["status"]="completed";search["action"]["type"]="search";
    auto source=search["action"]["sources"].createNestedObject();source["type"]="url";source["url"]="https://example.org/local-introduction";source["title"]="Local sample (synthetic)";
    auto message=output.createNestedObject();message["type"]="message";message["role"]="assistant";message["status"]="completed";
    auto content=message["content"].createNestedObject();content["type"]="output_text";content["text"]=static_cast<const char*>(workspace_.request);content["annotations"].to<JsonArray>();
    reply.status=200;
    if(doc.overflowed()||measureJson(doc)>capacity){reply.tooLarge=true;return Poll::Complete;}
    reply.bytes=serializeJson(doc,body,capacity);return Poll::Complete;
}
void MockSession::clock(uint32_t tick){if(!initialized_){lastTick_=tick;initialized_=true;}else{now_+=uint32_t(tick-lastTick_);lastTick_=tick;}transport_.now(now_);}
Outcome MockSession::begin(const Snapshot &s,uint32_t tick){clock(tick);transport_.prepare(s,now_);return engine_.begin(s,now_,true);}
bool MockSession::tick(uint32_t tick){clock(tick);auto before=engine_.outcome();engine_.poll(engine_.generation(),now_);return before!=engine_.outcome();}
}
#endif
