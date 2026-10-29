#pragma once
#include "intro_ai.h"
#include "intro_ai_http.h"
#include "intro_ai_settings.h"
#include <atomic>
namespace orbit_intro_ai {
enum class Open:uint8_t {Pending,Ready,Failed};
// Worker-only IO seam. The ESP32 implementation is real TLS; host tests supply
// fake DNS/TLS/streams. No callback may outlive its backend storage.
class SecureChannel {
public:
    virtual ~SecureChannel(){}
    virtual uint64_t nowMs()const=0;
    virtual uint64_t utcSeconds()const=0;
    virtual Outcome readiness()const=0; // Wi-Fi / trusted UTC / memory admission
    virtual Open open(const ApiSettings &,uint64_t deadline)=0;
    virtual Outcome failure()const=0;
    virtual int write(const uint8_t *,size_t)=0; //0=would block, negative=failed
    virtual int available()=0;
    virtual int read(uint8_t *,size_t)=0;
    virtual bool connected()=0;
    virtual void close()=0;
};
// All Transport methods run on ONE background owner, never LVGL. poll may spend
// up to a bounded SDK connect/handshake/write slice. UI cancellation only sets an
// atomic latch; buffers remain owned until worker poll/cancel releases the job.
class HttpsTransport:public Transport {
public:
    HttpsTransport(SecureChannel &,ApiSettings);
    ~HttpsTransport(){cancel();}
    bool start(const char *,const char *,size_t,uint32_t,uint32_t)override;
    Poll poll(char *,size_t,HttpResult &)override;
    void cancel()override;
    Outcome startFailure()const override{return error_;}
    void requestCancel(){cancelRequested_.store(true);}
    void limitDeadline(uint64_t absoluteDeadline){deadlineLimit_=absoluteDeadline;} // worker only;0=standalone default
private:
    enum Phase:uint8_t {Idle,Opening,Header,Payload,Receiving};
    SecureChannel &channel_;ApiSettings settings_;
    HttpResponse parser_;
    const char *request_=nullptr;
    char *response_=nullptr;size_t responseCapacity_=0;
    char headers_[1024]={};uint8_t input_[512];
    size_t headerSize_=0,requestSize_=0,sent_=0;
    uint64_t deadline_=0,deadlineLimit_=0,lastData_=0,headersAt_=0;
    uint32_t generation_=0;
    Phase phase_=Idle;
    Outcome error_=Outcome::TransportError;
    std::atomic<bool> cancelRequested_{false};
    bool sawData_=false;
    void wipeHeaders();
    Poll fail(Outcome,HttpResult &);
};
}
