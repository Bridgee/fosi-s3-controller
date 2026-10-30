#include "intro_ai.h"
#include "intro_ai_contract.h"
#include <ArduinoJson.h>
#include <string.h>
#include <limits.h>
#include <initializer_list>

namespace orbit_intro_ai {
namespace {
// Both JSON documents borrow fixed caller arenas. Never fall back to heap.
struct Arena {
    void *data;size_t capacity;
    void *allocate(size_t n){return n<=capacity?data:nullptr;}
    void deallocate(void *){}
    void *reallocate(void *,size_t){return nullptr;}
};
using Document=BasicJsonDocument<Arena>;
constexpr size_t StructuredBytes=4096;
static_assert(RequestBytes-StructuredBytes>=4096,"Reserve a bounded inner JSON pool");
bool eq(JsonVariantConst v,const char *s){if(!v.is<const char*>())return false;auto t=v.as<JsonString>();return t.size()==strlen(s)&&!memcmp(t.c_str(),s,t.size());}
bool utf8(const char *p,size_t n,bool spaces=false){
    if(!p)return false;
    for(size_t i=0;i<n;){
        unsigned char c=static_cast<unsigned char>(p[i++]);
        if(c<0x80){if(c<32 && !(spaces && (c==9||c==10||c==13)))return false;if(c==127)return false;continue;}
        uint32_t u=0;unsigned tail=0;
        if(c>=0xc2&&c<=0xdf){u=c&31;tail=1;}
        else if(c>=0xe0&&c<=0xef){u=c&15;tail=2;}
        else if(c>=0xf0&&c<=0xf4){u=c&7;tail=3;}else return false;
        if(i+tail>n)return false;
        for(unsigned j=0;j<tail;++j){unsigned char t=static_cast<unsigned char>(p[i++]);if((t&0xc0)!=0x80)return false;u=(u<<6)|(t&63);}
        if((tail==1&&u<0x80)||(tail==2&&u<0x800)||(tail==3&&u<0x10000)||u>0x10ffff||(u>=0xd800&&u<=0xdfff)||
           (u>=0x80&&u<=0x9f)||u==0x2028||u==0x2029||(u>=0x202a&&u<=0x202e)||(u>=0x2066&&u<=0x2069))return false;
    }
    return true;
}
bool field(const char *p,size_t cap){size_t n=0;while(n<cap&&p[n])++n;return n<cap&&utf8(p,n);}
bool snapshotValid(const Snapshot &s){
    if(!field(s.title,sizeof s.title)||!field(s.artist,sizeof s.artist)||!field(s.release,sizeof s.release)||s.durationSeconds>86400)return false;
    if(s.topic!=Topic::Artist&&s.topic!=Topic::Release&&s.topic!=Topic::Song)return false;
    if(!present(s.artist))return false;
    return s.topic==Topic::Artist || present(s.title) || (s.topic==Topic::Release&&present(s.release));
}
bool sameTrack(const Snapshot &a,const Snapshot &b){return a.durationSeconds==b.durationSeconds&&!strcmp(a.title,b.title)&&!strcmp(a.artist,b.artist)&&!strcmp(a.release,b.release);}
bool copyText(JsonVariantConst v,char *dest,size_t cap,bool nonempty=true){
    if(!v.is<const char*>())return false;auto s=v.as<JsonString>();
    if(s.size()>=cap||!utf8(s.c_str(),s.size())||(nonempty&&!present(s.c_str())))return false;
    memcpy(dest,s.c_str(),s.size());dest[s.size()]=0;return true;
}
bool urlValid(const char *s){
    const size_t n=strlen(s);if(n<12||n>=sizeof(Reference::url)||strncmp(s,"https://",8))return false;
    const char *h=s+8,*end=h;bool dot=false,letter=false;
    while(*end&&*end!='/'&&*end!='?'&&*end!='#'){
        const char c=*end++;if(c=='.')dot=true;else if((c>='a'&&c<='z')||(c>='A'&&c<='Z'))letter=true;
        else if(!((c>='0'&&c<='9')||c=='-'))return false;
    }
    if(!dot||!letter||end==h||*h=='.'||end[-1]=='.')return false;
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(s);*p;++p)if(*p<=32||*p==127||*p=='\\'||*p=='"'||*p=='<'||*p=='>')return false;
    return true;
}
// Reject trailing data, NULs and unmatched containers before ArduinoJson's
// permissive first-value parser. Actual token syntax is checked by ArduinoJson.
bool completeObject(const char *p,size_t n){
    if(!n||!utf8(p,n,true))return false;
    bool quote=false,escape=false,started=false,ended=false;char nesting[16];unsigned depth=0;
    for(size_t i=0;i<n;++i){char c=p[i];if(ended){if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n')return false;continue;}
        if(quote){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quote=false;continue;}
        if(!started){if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;if(c!='{')return false;started=true;}
        if(c=='"')quote=true;
        else if(c=='{'||c=='['){if(depth==sizeof nesting)return false;nesting[depth++]=c;}
        else if(c=='}'||c==']'){if(!depth||nesting[--depth]!=(c=='}'?'{':'['))return false;if(!depth)ended=true;}
    }
    return started&&ended&&!quote&&!escape;
}
bool buildRequest(const Snapshot &s,Workspace &w){
    Document doc(JsonPoolBytes,Arena{w.jsonPool,JsonPoolBytes});
    doc["topic"]=s.topic==Topic::Artist?"Artist":s.topic==Topic::Release?"Release":"Song";
    doc["title"]=static_cast<const char*>(s.title);doc["artist"]=static_cast<const char*>(s.artist);doc["release"]=static_cast<const char*>(s.release);
    if(s.durationSeconds)doc["duration_seconds"]=s.durationSeconds;else doc["duration_seconds"]=nullptr;
    if(doc.overflowed()||measureJson(doc)>=sizeof w.response)return false;
    const size_t metaBytes=serializeJson(doc,w.response,sizeof w.response);w.response[metaBytes]=0;
    doc.clear();
    doc["model"]=Model;doc["reasoning"]["effort"]="none";
    doc["store"]=false;doc["stream"]=false;doc["background"]=false;
    doc["max_output_tokens"]=MaxOutputTokens;doc["max_tool_calls"]=1;doc["parallel_tool_calls"]=false;
    doc["instructions"]=OpenaiInstructions;
    doc["input"]=static_cast<const char*>(w.response);
    auto tool=doc["tools"].createNestedObject();tool["type"]="web_search";tool["search_context_size"]="low";
    doc["tool_choice"]="required";
    doc["include"].add("web_search_call.action.sources");
    auto format=doc["text"]["format"];format["type"]="json_schema";format["name"]="music_intro_v2";format["strict"]=true;
    format["schema"]=serialized(OpenaiSchema);
    if(doc.overflowed()||measureJson(doc)>=sizeof w.request)return false;
    const size_t bytes=serializeJson(doc,w.request,sizeof w.request);w.request[bytes]=0;return true;
}
bool validReleaseType(JsonVariantConst v){for(const char *s:{"album","ep","single","compilation","other","unknown"})if(eq(v,s))return true;return false;}
// Only accept URLs actually supplied by a completed search or URL annotation.
// Membership is provenance, not proof that every generated claim is entailed.
// A low-context search can return more than32 URLs. Scan the complete parsed
// arrays: response bytes and the fixed JSON arena already bound work/memory.
// Never truncate the list, which could discard a cited URL near its end.
bool sourceTitle(JsonArrayConst output,const char *url,char *title,size_t cap){
    JsonVariantConst found;bool match=false;
    for(auto item:output){
        if(eq(item["type"],"web_search_call")){
            auto a=item["action"]["sources"].as<JsonArrayConst>();
            for(auto s:a)if(eq(s["type"],"url")&&eq(s["url"],url)){match=true;if(s["title"].is<const char*>())found=s["title"];}
        }else if(eq(item["type"],"message"))for(auto c:item["content"].as<JsonArrayConst>()){
            auto a=c["annotations"].as<JsonArrayConst>();
            for(auto s:a)if(eq(s["type"],"url_citation")&&eq(s["url"],url)){match=true;if(s["title"].is<const char*>())found=s["title"];}
        }
    }
    if(!match)return false;
    if(!found.isNull()){
        // Long titles are shortened only at valid UTF-8 boundaries. No URL/text
        // is ever truncated to turn invalid evidence into a valid reference.
        auto s=found.as<JsonString>();if(!utf8(s.c_str(),s.size()))return false;
        size_t n=s.size()<cap-1?s.size():cap-1;
        while(n&&(static_cast<unsigned char>(s.c_str()[n])&0xc0)==0x80)--n;
        memcpy(title,s.c_str(),n);title[n]=0;
    }
    if(!title[0]){size_t n=strcspn(url+8,"/?#");if(n>=cap)n=cap-1;memcpy(title,url+8,n);title[n]=0;}
    return true;
}
Outcome parseResponse(Workspace &w,size_t bytes,Introduction &out){
    if(bytes>MaxResponseBytes)return Outcome::TooLarge;
    if(!completeObject(w.response,bytes))return Outcome::Malformed;
    Document envelope(JsonPoolBytes,Arena{w.jsonPool,JsonPoolBytes});
    auto error=deserializeJson(envelope,w.response,bytes,DeserializationOption::NestingLimit(12));
    if(error)return error==DeserializationError::NoMemory?Outcome::TooLarge:Outcome::Malformed;
    auto root=envelope.as<JsonVariantConst>();
    if(!root["error"].isNull())return Outcome::HttpError;
    if(!eq(root["status"],"completed")||!root["incomplete_details"].isNull())return Outcome::Incomplete;
    auto output=root["output"].as<JsonArrayConst>();if(!root["output"].is<JsonArrayConst>()||!output.size()||output.size()>8)return Outcome::Malformed;
    JsonVariantConst text;unsigned calls=0,messages=0;
    for(auto item:output){
        if(eq(item["type"],"web_search_call")){
            if(++calls>1||!eq(item["status"],"completed"))return Outcome::ToolError;
        }else if(eq(item["type"],"message")){
            if(++messages>1||!eq(item["role"],"assistant"))return Outcome::Malformed;
            if(!eq(item["status"],"completed"))return Outcome::Incomplete;
            auto content=item["content"].as<JsonArrayConst>();if(!content.size()||content.size()>8)return Outcome::Malformed;
            for(auto c:content){if(eq(c["type"],"refusal"))return Outcome::Refused;
                if(!eq(c["type"],"output_text")||!text.isNull()||!c["text"].is<const char*>())return Outcome::Malformed;text=c["text"];}
        }else if(!eq(item["type"],"reasoning"))return Outcome::ToolError;
    }
    if(calls!=1)return Outcome::ToolError;
    if(text.isNull())return Outcome::Malformed;
    auto compact=text.as<JsonString>();if(compact.size()>=StructuredBytes)return Outcome::TooLarge;
    if(!completeObject(compact.c_str(),compact.size()))return Outcome::Malformed;
    memcpy(w.request,compact.c_str(),compact.size());w.request[compact.size()]=0;
    Document intro(RequestBytes-StructuredBytes,Arena{w.request+StructuredBytes,RequestBytes-StructuredBytes});
    error=deserializeJson(intro,w.request,compact.size(),DeserializationOption::NestingLimit(6));
    if(error)return error==DeserializationError::NoMemory?Outcome::TooLarge:Outcome::Malformed;
    auto v=intro.as<JsonObjectConst>();
    if(v.isNull()||v.size()!=4||!v.containsKey("status")||!v.containsKey("subject")||!v.containsKey("release_type")||!v.containsKey("paragraphs")||
       !validReleaseType(v["release_type"])||!v["paragraphs"].is<JsonArrayConst>())return Outcome::Malformed;
    auto paragraphs=v["paragraphs"].as<JsonArrayConst>();
    if(!eq(v["status"],"ready")){
        if(paragraphs.size()||!eq(v["subject"],"")||!eq(v["release_type"],"unknown"))return Outcome::Malformed;
        if(eq(v["status"],"ambiguous"))return Outcome::Ambiguous;
        if(eq(v["status"],"not_found"))return Outcome::NotFound;
        return eq(v["status"],"insufficient_evidence")?Outcome::InsufficientEvidence:Outcome::Malformed;
    }
    if(paragraphs.size()!=2||!copyText(v["subject"],out.subject,sizeof out.subject)||!copyText(v["release_type"],out.releaseType,sizeof out.releaseType))return Outcome::Malformed;
    size_t length=0;unsigned words=0,index=0;
    for(auto p:paragraphs){
        if(!p.is<JsonObjectConst>()||p.size()!=2||!p["text"].is<const char*>()||!p["source_urls"].is<JsonArrayConst>())return Outcome::Malformed;
        auto t=p["text"].as<JsonString>();auto urls=p["source_urls"].as<JsonArrayConst>();
        if(!t.size()||!utf8(t.c_str(),t.size())||!present(t.c_str())||!urls.size()||urls.size()>MaxReferences)return Outcome::Malformed;
        if(index){if(length+2>MaxIntroTextBytes)return Outcome::TooLarge;out.text[length++]='\n';out.text[length++]='\n';}
        out.paragraphStart[index]=uint16_t(length);
        if(t.size()>MaxIntroTextBytes-length)return Outcome::TooLarge;
        bool inWord=false;for(size_t j=0;j<t.size();++j){bool space=t.c_str()[j]==' ';if(!space&&!inWord)++words;inWord=!space;}
        if(words>110)return Outcome::TooLarge;
        memcpy(out.text+length,t.c_str(),t.size());length+=t.size();out.text[length]=0;
        for(auto u:urls){
            if(!u.is<const char*>())return Outcome::Malformed;auto s=u.as<JsonString>();
            if(!utf8(s.c_str(),s.size())||strlen(s.c_str())!=s.size()||!urlValid(s.c_str()))return Outcome::Malformed;
            unsigned ref=0;while(ref<out.sourceCount&&strcmp(out.sources[ref].url,s.c_str()))++ref;
            if(ref==out.sourceCount){
                if(ref==MaxReferences)return Outcome::TooLarge;
                if(!sourceTitle(output,s.c_str(),out.sources[ref].title,sizeof out.sources[ref].title))return Outcome::Malformed;
                memcpy(out.sources[ref].url,s.c_str(),s.size()+1);++out.sourceCount;
            }
            if(out.paragraphSources[index]&(1u<<ref))return Outcome::Malformed;
            out.paragraphSources[index]|=uint8_t(1u<<ref);
        }
        ++index;
    }
    return Outcome::Ready;
}
}
bool capture(Snapshot &out,Topic topic,const char *title,const char *artist,const char *release,uint32_t duration){
    Snapshot next;next.topic=topic;next.durationSeconds=duration;
    const char *values[]={title?title:"",artist?artist:"",release?release:""};
    char *dest[]={next.title,next.artist,next.release};const size_t caps[]={sizeof next.title,sizeof next.artist,sizeof next.release};
    for(unsigned i=0;i<3;++i){size_t n=0;while(n<caps[i]&&values[i][n])++n;if(n==caps[i]||!utf8(values[i],n))return false;memcpy(dest[i],values[i],n+1);}
    if(!snapshotValid(next))return false;out=next;return true;
}
Engine::Engine(Transport &t,Workspace &w):transport_(t),workspace_(w){}
Engine::~Engine(){cancel();}
void Engine::clearCache(){memset(static_cast<void*>(cache_),0,sizeof cache_);}
void Engine::trackChanged(){clearCache();trackReleased_=true;fromCache_=false;if(outcome_==Outcome::Pending)cancel();}
void Engine::discardCurrentResult(){cache_[unsigned(snapshot_.topic)].valid=false;cancel();}
void Engine::configurationChanged(){cancel();clearCache();configBlocked_=false;blockedMs_=0;outcome_=Outcome::Idle;}
void Engine::cancel(){if(outcome_==Outcome::Pending)transport_.cancel();++generation_;outcome_=Outcome::Canceled;fromCache_=false;}
uint32_t Engine::retryInMs(uint64_t now)const{
    if(now<blockedAt_||now-blockedAt_>=blockedMs_)return 0;
    uint64_t left=blockedMs_-(now-blockedAt_);return left>UINT32_MAX?UINT32_MAX:uint32_t(left);
}
bool Engine::loadCache(uint64_t now){
    auto &e=cache_[unsigned(snapshot_.topic)];if(!e.valid)return false;
    // Completed text lasts for this track session. Only negative results expire.
    if(now<e.inserted || (e.outcome!=Outcome::Ready && now-e.inserted>=MissTtlMs)){e.valid=false;return false;}
    value_=e.value;outcome_=e.outcome;fromCache_=true;return true;
}
void Engine::saveCache(uint64_t now){
    if(trackReleased_ || (outcome_!=Outcome::Ready&&outcome_!=Outcome::Ambiguous&&outcome_!=Outcome::NotFound&&outcome_!=Outcome::InsufficientEvidence))return;
    auto &e=cache_[unsigned(snapshot_.topic)];e.value=value_;e.outcome=outcome_;e.inserted=now;e.valid=true;
}
void Engine::finish(Outcome o,uint64_t now,uint64_t cooldown){
    if(outcome_==Outcome::Pending)transport_.cancel();outcome_=o;
    if(o!=Outcome::Ready)memset(static_cast<void*>(&value_),0,sizeof value_);
    if(cooldown){blockedAt_=now;blockedMs_=cooldown;blockedReason_=o;}
    saveCache(now);
}
Outcome Engine::begin(const Snapshot &s,uint64_t now,bool online){
    if(outcome_==Outcome::Pending)return Outcome::Busy;
    ++generation_;fromCache_=false;memset(static_cast<void*>(&value_),0,sizeof value_);
    if(!snapshotValid(s))return outcome_=Outcome::InvalidMetadata;
    if(trackReleased_ || !sameTrack(snapshot_,s))clearCache();
    snapshot_=s;trackReleased_=false;if(loadCache(now))return outcome_;
    if(!online)return outcome_=Outcome::Offline;
    if(configBlocked_)return outcome_=Outcome::ConfigError;
    if(retryInMs(now))return outcome_=blockedReason_;
    if(!buildRequest(s,workspace_))return outcome_=Outcome::TooLarge;
    started_=now;blockedAt_=now;blockedMs_=MinRequestGapMs;blockedReason_=Outcome::RateLimited;
    outcome_=Outcome::Pending;
    if(!transport_.start(Endpoint,workspace_.request,strlen(workspace_.request),generation_,DeadlineMs)){const auto failure=transport_.startFailure();finish(failure,now,failure==Outcome::Canceled?0:FailureCooldownMs);}
    return outcome_;
}
Outcome Engine::poll(uint32_t token,uint64_t now){
    if(token!=generation_||outcome_!=Outcome::Pending)return outcome_;
    if(now<started_||now-started_>=DeadlineMs){finish(Outcome::Timeout,now,FailureCooldownMs);return outcome_;}
    HttpResult reply;auto state=transport_.poll(workspace_.response,MaxResponseBytes,reply);
    if(state==Poll::Pending)return outcome_;
    if(reply.generation!=generation_)return outcome_; // late callback cannot publish
    if(state==Poll::Failed){finish(reply.failure,now,reply.failure==Outcome::Canceled?0:FailureCooldownMs);return outcome_;}
    if(reply.status==401||reply.status==403){configBlocked_=true;finish(Outcome::ConfigError,now);return outcome_;}
    if(reply.status==429){uint64_t wait=reply.retryAfterSeconds?uint64_t(reply.retryAfterSeconds)*1000:60000;finish(Outcome::RateLimited,now,wait);return outcome_;}
    if(reply.status!=200){uint64_t wait=FailureCooldownMs;if(reply.status==503&&uint64_t(reply.retryAfterSeconds)*1000>wait)wait=uint64_t(reply.retryAfterSeconds)*1000;finish(Outcome::HttpError,now,wait);return outcome_;}
    if(reply.tooLarge||reply.bytes>MaxResponseBytes){finish(Outcome::TooLarge,now,FailureCooldownMs);return outcome_;}
    if(reply.truncated){finish(Outcome::Incomplete,now,FailureCooldownMs);return outcome_;}
    auto parsed=parseResponse(workspace_,reply.bytes,value_);
    // release_type is irrelevant for Artist/Song and must not leak an inferred
    // release into those contexts or a later Release cache entry.
    if(parsed==Outcome::Ready&&snapshot_.topic!=Topic::Release&&strcmp(value_.releaseType,"unknown"))parsed=Outcome::Malformed;
    finish(parsed,now,parsed==Outcome::Ready||parsed==Outcome::Ambiguous||parsed==Outcome::NotFound||parsed==Outcome::InsufficientEvidence?0:FailureCooldownMs);
    return outcome_;
}
const char *Engine::message(Outcome o){
    switch(o){case Outcome::Ready:return "";case Outcome::Pending:return "Loading";case Outcome::Busy:return "Please wait";
    case Outcome::Ambiguous:return "Identity unclear";case Outcome::NotFound:return "No introduction found";
    case Outcome::InsufficientEvidence:return "Not enough information";case Outcome::InvalidMetadata:case Outcome::Unavailable:return "Track information unavailable";
    case Outcome::Disabled:return "Introductions are off";case Outcome::ClockUntrusted:return "Waiting for time sync";case Outcome::ResourceLimited:return "Try again later";case Outcome::TlsError:return "Secure connection failed";case Outcome::ProtocolError:return "Could not load introduction";
    case Outcome::Offline:return "Connect to Wi-Fi";case Outcome::ConfigError:return "Check AI settings";case Outcome::RateLimited:return "Try again later";
    case Outcome::Timeout:return "Request timed out";case Outcome::Refused:return "Introduction unavailable";case Outcome::Canceled:case Outcome::Idle:return "";
    default:return "Could not load introduction";}
}
}
