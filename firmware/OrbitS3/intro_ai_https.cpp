#include "intro_ai_https.h"
#include <string.h>
#include <stdio.h>
namespace orbit_intro_ai {
HttpsTransport::HttpsTransport(SecureChannel &channel,ApiSettings settings):channel_(channel),settings_(settings){}
void HttpsTransport::wipeHeaders(){volatile char *p=headers_;for(size_t i=0;i<sizeof headers_;++i)p[i]=0;headerSize_=0;}
void HttpsTransport::cancel(){channel_.close();phase_=Idle;request_=nullptr;response_=nullptr;responseCapacity_=0;requestSize_=sent_=0;deadlineLimit_=0;wipeHeaders();volatile uint8_t *wipe=input_;for(size_t i=0;i<sizeof input_;++i)wipe[i]=0;parser_.reset(nullptr,0);cancelRequested_.store(false);}
Poll HttpsTransport::fail(Outcome error,HttpResult &r){r=HttpResult{};r.generation=generation_;r.failure=error;error_=error;cancel();return Poll::Failed;}
bool HttpsTransport::start(const char *endpoint,const char *request,size_t bytes,uint32_t generation,uint32_t timeout){
    if(phase_!=Idle){error_=Outcome::Busy;return false;}
    const auto now=channel_.nowMs();
    if(deadlineLimit_&&now>=deadlineLimit_){error_=Outcome::Timeout;return false;}
    const auto config=validateSettings(settings_);
    if(config!=SettingsStatus::Ready){error_=config==SettingsStatus::Disabled?Outcome::Disabled:Outcome::ConfigError;return false;}
    if(!endpoint||strcmp(endpoint,Endpoint)||!request||!bytes||bytes>=RequestBytes||!timeout||timeout>DeadlineMs){error_=Outcome::ConfigError;return false;}
    error_=channel_.readiness();if(error_!=Outcome::Idle)return false;
    generation_=generation;deadline_=now+timeout;if(deadlineLimit_&&deadlineLimit_<deadline_)deadline_=deadlineLimit_;request_=request;requestSize_=bytes;sent_=0;
    const int size=snprintf(headers_,sizeof headers_,"POST /v1/responses HTTP/1.1\r\nHost: api.openai.com\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\nContent-Length: %u\r\n\r\n",settings_.key,unsigned(bytes));
    if(size<=0||size_t(size)>=sizeof headers_){wipeHeaders();request_=nullptr;error_=Outcome::ConfigError;return false;}
    headerSize_=size_t(size);phase_=Opening;cancelRequested_.store(false);sawData_=false;headersAt_=lastData_=0;return true;
}
Poll HttpsTransport::poll(char *body,size_t capacity,HttpResult &r){
    if(phase_==Idle)return Poll::Pending;
    if(cancelRequested_.load())return fail(Outcome::Canceled,r);
    uint64_t now=channel_.nowMs();if(now>=deadline_)return fail(Outcome::Timeout,r);
    auto ready=channel_.readiness();if(ready==Outcome::Offline||ready==Outcome::ClockUntrusted)return fail(ready,r);
    if(!body||capacity>MaxResponseBytes||!capacity)return fail(Outcome::ProtocolError,r);
    if(phase_==Receiving && (body!=response_||capacity!=responseCapacity_))return fail(Outcome::ProtocolError,r);
    if(phase_==Opening){
        if(ready!=Outcome::Idle)return fail(ready,r);
        const auto open=channel_.open(settings_,deadline_);
        if(cancelRequested_.load())return fail(Outcome::Canceled,r);
        if(channel_.nowMs()>=deadline_)return fail(Outcome::Timeout,r);
        if(open==Open::Failed)return fail(channel_.failure(),r);
        if(open==Open::Pending)return Poll::Pending;
        phase_=Header;sent_=0;
    }
    if(phase_==Header||phase_==Payload){
        // Nonblocking TLS writes preserve the exact pointer/size across WANT_*
        // retries; no second connection or full POST replay is attempted.
        const bool header=phase_==Header;const char *data=header?headers_:request_;const size_t size=header?headerSize_:requestSize_;
        size_t amount=size-sent_;if(amount>512)amount=512;
        const auto count=channel_.write(reinterpret_cast<const uint8_t*>(data)+sent_,amount);
        if(count<0||size_t(count)>amount)return fail(Outcome::TransportError,r);
        if(cancelRequested_.load())return fail(Outcome::Canceled,r);
        if(channel_.nowMs()>=deadline_)return fail(Outcome::Timeout,r);
        if(!count)return Poll::Pending; // Same unsent TLS fragment; total deadline still applies.
        sent_+=size_t(count);if(sent_!=size)return Poll::Pending;
        sent_=0;
        if(header){wipeHeaders();phase_=Payload;return Poll::Pending;}
        phase_=Receiving;response_=body;responseCapacity_=capacity;lastData_=channel_.nowMs();parser_.reset(body,capacity,channel_.utcSeconds());
    }
    if(phase_==Receiving){
        now=channel_.nowMs();
        if(sawData_&&!parser_.headersReceived()&&now-headersAt_>=2000)return fail(Outcome::Timeout,r);
        if(parser_.headersReceived()&&now-lastData_>=5000)return fail(Outcome::Timeout,r);
        int count=channel_.available();
        if(count<0)return fail(Outcome::TransportError,r);
        if(count>0){size_t amount=size_t(count);if(amount>sizeof input_)amount=sizeof input_;int got=channel_.read(input_,amount);
            if(got<0||size_t(got)>amount)return fail(Outcome::TransportError,r);
            if(got){if(!sawData_){sawData_=true;headersAt_=now;}lastData_=now;
                if(!parser_.feed(input_,size_t(got)))return fail(parser_.error(),r);}
        }else if(!channel_.connected())parser_.eof();
        if(cancelRequested_.load())return fail(Outcome::Canceled,r);
        if(channel_.nowMs()>=deadline_)return fail(Outcome::Timeout,r);
        if(parser_.error()!=Outcome::Idle)return fail(parser_.error(),r);
        if(parser_.done()){r=parser_.result();r.generation=generation_;cancel();return Poll::Complete;}
    }
    return Poll::Pending;
}
}
