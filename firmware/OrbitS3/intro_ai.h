#pragma once
#include "intro_ai_policy.h"
#include <stddef.h>
#include <stdint.h>

// Core owns no credentials, global instances or UI callbacks. The separate
// default-disabled HTTPS adapter runs on a background owner; mocks stay local.
namespace orbit_intro_ai {
constexpr const char *Model="gpt-6.1-terra";
constexpr const char *Endpoint="https://api.openai.com/v1/responses";
constexpr uint16_t ContractVersion=2;
constexpr size_t RequestBytes=8192, JsonPoolBytes=16384;
constexpr uint32_t DeadlineMs=30000, MissTtlMs=10u*60*1000;
constexpr uint32_t MinRequestGapMs=1500, FailureCooldownMs=5000;
constexpr unsigned MaxOutputTokens=768;

enum class Outcome:uint8_t {Idle,Pending,Ready,Ambiguous,NotFound,InsufficientEvidence,
    Unavailable,InvalidMetadata,Busy,Offline,ConfigError,RateLimited,Timeout,
    TransportError,HttpError,TooLarge,Malformed,Incomplete,Refused,ToolError,Canceled,Disabled,ClockUntrusted,ResourceLimited,TlsError,ProtocolError};
struct Snapshot {
    Topic topic=Topic::Song;
    char title[256]={},artist[192]={},release[256]={};
    uint32_t durationSeconds=0;
};
bool capture(Snapshot &,Topic,const char *title,const char *artist,const char *release,uint32_t durationSeconds=0);
struct Reference {char url[384]={},title[96]={};};
struct Introduction {
    char subject[160]={},releaseType[16]={},text[MaxIntroTextBytes+1]={};
    Reference sources[MaxReferences];
    uint16_t paragraphStart[2]={};
    uint8_t paragraphSources[2]={},sourceCount=0;
};
// Time arguments are 64-bit monotonic milliseconds (e.g. esp_timer_get_time/1000),
// never SNTP wall time or unextended millis().
// Background runtime allocates lazily in PSRAM. Never put this on a task stack.
// Workspace and Transport must both outlive Engine. Allocate the whole feature
// lazily; this reference-based core does not free/rebind its workspace per call.
struct Workspace {
    alignas(void*) char request[RequestBytes]={},response[MaxResponseBytes+1]={};
    alignas(void*) unsigned char jsonPool[JsonPoolBytes];
};
struct HttpResult {
    int status=0;
    uint32_t generation=0,retryAfterSeconds=0;
    size_t bytes=0;
    bool truncated=false,tooLarge=false;
    Outcome failure=Outcome::TransportError;
};
enum class Poll:uint8_t {Pending,Complete,Failed};
class Transport {
public:
    virtual ~Transport() {}
    // Start/poll must not block the UI; future HTTPS work belongs on a worker.
    // Endpoint is fixed. Disable redirects, validate CA/hostname, do not log keys.
    // Request stays valid until completion/cancel. Copy no credentials into it.
    virtual bool start(const char *endpoint,const char *request,size_t bytes,uint32_t generation,uint32_t deadlineMs)=0;
    // Write at most capacity bytes, report overflow/truncation. Bind every reply
    // to its generation. Never silently accept an oversized/truncated prefix.
    virtual Poll poll(char *body,size_t capacity,HttpResult &result)=0;
    // Must stop/release the local job before the workspace can be reused. This
    // cannot guarantee cancellation of an already-billed server-side request.
    virtual void cancel()=0;
    virtual Outcome startFailure() const {return Outcome::TransportError;}
};
class Engine {
public:
    Engine(Transport &,Workspace &);
    ~Engine();
    Engine(const Engine &)=delete;
    Engine &operator=(const Engine &)=delete;
    Outcome begin(const Snapshot &,uint64_t now,bool online);
    Outcome poll(uint32_t generation,uint64_t now);
    void cancel();
    void clearCache();
    void trackChanged(); // owner observes confirmed track/source change; preserves an open ready reader
    void discardCurrentResult(); // canceled completion must not evict other topics
    void configurationChanged(); // owner explicitly corrected key/config
    Outcome outcome() const {return outcome_;}
    uint32_t generation() const {return generation_;}
    bool fromCache() const {return fromCache_;}
    const Introduction *result() const {return outcome_==Outcome::Ready?&value_:nullptr;}
    uint32_t retryInMs(uint64_t now) const;
    static const char *message(Outcome);
private:
    struct Entry {
        Introduction value;
        uint64_t inserted=0;
        Outcome outcome=Outcome::Idle;
        bool valid=false;
    };
    Transport &transport_;Workspace &workspace_;
    Entry cache_[3]; // Artist / Release / Song, one shared exact track identity
    Snapshot snapshot_;
    Introduction value_;
    uint32_t generation_=0;
    uint64_t started_=0,blockedAt_=0,blockedMs_=0;
    Outcome outcome_=Outcome::Idle,blockedReason_=Outcome::RateLimited;
    bool fromCache_=false,configBlocked_=false,trackReleased_=true;
    void finish(Outcome,uint64_t now,uint64_t cooldown=0);
    bool loadCache(uint64_t now);
    void saveCache(uint64_t now);
};
}
