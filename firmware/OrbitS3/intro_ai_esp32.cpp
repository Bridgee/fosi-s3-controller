#include "config.h"
#include "intro_ai_esp32.h"
#include "intro_ai_worker.h"
#if ORBIT_AI_ENABLED || ORBIT_AI_TRANSPORT_CHECK || defined(ORBIT_AI_HOST_TEST)
#if defined(ORBIT_AI_HOST_TEST)
#if defined(ARDUINO)
#error "Fake AI platform is host-test only"
#endif
#include "ai_platform_fake.h"
#else
#include "network_clock.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <ssl_client.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#endif
#include <atomic>
#include <new>
#include <string.h>
namespace orbit_intro_ai {
namespace {
// DNS callback storage is static and is never freed on timeout/cancel. This
// avoids WiFi.hostByName's long blocking wait and a late callback into a stack
// variable. Do not reuse the slot until its sole callback completes. Only this
// fixed OpenAI hostname is ever resolved. IPv6-only networks fail safely.
struct DnsSlot {std::atomic<unsigned> state{0};uint32_t address=0;};
DnsSlot dnsSlot;
std::atomic<bool> channelAllocated{false};
void dnsFound(const char *,const ip_addr_t *address,void *){
    dnsSlot.address=address&&IP_IS_V4(address)?ip4_addr_get_u32(ip_2_ip4(address)):0;
    dnsSlot.state.store(2);
}
void resolveOnTcpip(void *){
    ip_addr_t address;const auto error=dns_gethostbyname("api.openai.com",&address,dnsFound,nullptr);
    if(error==ERR_OK)dnsFound(nullptr,&address,nullptr);
    else if(error!=ERR_INPROGRESS)dnsFound(nullptr,nullptr,nullptr);
}
class Esp32Channel:public SecureChannel {
    sslclient_context *tls_=nullptr;
    uint64_t dnsStarted_=0;
    uint8_t input_[512];size_t inputSize_=0,inputOffset_=0;
    const char *ca_=nullptr;
    bool opening_=false,eof_=false;
    Outcome error_=Outcome::TransportError;
public:
    ~Esp32Channel()override{close();channelAllocated.store(false);}
    uint64_t nowMs()const override{return uint64_t(esp_timer_get_time())/1000;}
    uint64_t utcSeconds()const override{return uint64_t(time(nullptr));}
    Outcome readiness()const override{
#if ORBIT_AI_TRANSPORT_CHECK || (!defined(ORBIT_AI_HOST_TEST) && (!ORBIT_AI_ENABLED || defined(ORBIT_BUILD_CHECK)))
        return Outcome::Disabled;
#else
        if(WiFi.status()!=WL_CONNECTED)return Outcome::Offline;
        if(!orbit_clock::utc_ready())return Outcome::ClockUntrusted;
        if(!tls_&&(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<192*1024||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<96*1024))return Outcome::ResourceLimited;
        return Outcome::Idle;
#endif
    }
    Outcome failure()const override{return error_;}
    Open open(const ApiSettings &settings,uint64_t deadline)override{
        error_=readiness();if(error_!=Outcome::Idle)return Open::Failed;
        if(validateSettings(settings)!=SettingsStatus::Ready){error_=Outcome::ConfigError;return Open::Failed;}
        if(tls_)return Open::Ready;
        const auto now=nowMs();if(now>=deadline){error_=Outcome::Timeout;return Open::Failed;}
        if(!opening_){
            // An older canceled DNS lookup may still own the static slot. It is
            // safe to join that same fixed-host lookup, never overwrite its data.
            unsigned empty=0;
            if(dnsSlot.state.compare_exchange_strong(empty,1)){
                if(tcpip_callback(resolveOnTcpip,nullptr)!=ERR_OK){dnsSlot.state.store(0);error_=Outcome::TransportError;return Open::Failed;}
            }
            dnsStarted_=now;opening_=true;
        }
        if(dnsSlot.state.load()!=2){if(now-dnsStarted_>=3000){error_=Outcome::Timeout;return Open::Failed;}return Open::Pending;}
        const uint32_t address=dnsSlot.address;dnsSlot.state.store(0);opening_=false;
        if(!address){error_=Outcome::TransportError;return Open::Failed;}
        // Reserve the full TCP(2s)+TLS(4s) SDK slices before entering either.
        if(deadline-now<7000){error_=Outcome::Timeout;return Open::Failed;}
        auto ready=readiness();if(ready!=Outcome::Idle){error_=ready;return Open::Failed;}
        tls_=static_cast<sslclient_context*>(heap_caps_malloc(sizeof(sslclient_context),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        if(!tls_){error_=Outcome::ResourceLimited;return Open::Failed;}
        // Use the pinned library's C TLS backend directly, avoiding an unchecked
        // nested new in WiFiClientSecure's constructor and hidden String copies.
        ssl_init(tls_);tls_->socket=-1;tls_->handshake_timeout=4000;ca_=settings.caPem;
        const int socket=start_ssl_client(tls_,IPAddress(address),443,"api.openai.com",2000,ca_,false,nullptr,nullptr,nullptr,nullptr,false,nullptr);
        if(socket<0||mbedtls_ssl_get_verify_result(&tls_->ssl_ctx)!=0){error_=Outcome::TlsError;close();return Open::Failed;}
        if(nowMs()>=deadline){error_=Outcome::Timeout;close();return Open::Failed;}
        eof_=false;return Open::Ready;
    }
    int write(const uint8_t *data,size_t n)override{
        if(!tls_||eof_)return -1;
        const int count=mbedtls_ssl_write(&tls_->ssl_ctx,data,n);
        if(count==MBEDTLS_ERR_SSL_WANT_READ||count==MBEDTLS_ERR_SSL_WANT_WRITE)return 0;
        if(count<=0){error_=Outcome::TransportError;eof_=true;return -1;}return count;
    }
    int available()override{
        if(inputOffset_<inputSize_)return int(inputSize_-inputOffset_);
        if(!tls_||eof_)return 0;
        // The SDK leaves the TCP socket O_NONBLOCK. WANT_* is yielded to the
        // worker instead of using Stream::readBytes or per-byte blocking waits.
        const int count=mbedtls_ssl_read(&tls_->ssl_ctx,input_,sizeof input_);
        if(count==MBEDTLS_ERR_SSL_WANT_READ||count==MBEDTLS_ERR_SSL_WANT_WRITE)return 0;
        if(count==0||count==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){eof_=true;return 0;}
        if(count<0){error_=Outcome::TransportError;eof_=true;return -1;}
        inputOffset_=0;inputSize_=size_t(count);return count;
    }
    int read(uint8_t *out,size_t n)override{
        if(n>inputSize_-inputOffset_)n=inputSize_-inputOffset_;
        if(n)memcpy(out,input_+inputOffset_,n);inputOffset_+=n;return int(n);
    }
    bool connected()override{return tls_&&!eof_;}
    void close()override{
        // Called only by the worker owner, never concurrently with TLS calls.
        if(tls_){stop_ssl_socket(tls_,ca_,nullptr,nullptr);free(tls_);tls_=nullptr;}
        volatile uint8_t *wipe=input_;for(size_t i=0;i<sizeof input_;++i)wipe[i]=0;
        inputOffset_=inputSize_=0;opening_=false;eof_=false;ca_=nullptr;
    }
};
}
SecureChannel *createEsp32Channel(){
#if ORBIT_AI_TRANSPORT_CHECK || (!defined(ORBIT_AI_HOST_TEST) && (!ORBIT_AI_ENABLED || defined(ORBIT_BUILD_CHECK)))
    return nullptr;
#else
    bool expected=false;if(!channelAllocated.compare_exchange_strong(expected,true))return nullptr;
    auto *channel=new(std::nothrow) Esp32Channel;if(!channel)channelAllocated.store(false);return channel;
#endif
}
void destroyEsp32Channel(SecureChannel *channel){delete channel;}
// Kept by a verification-only linker flag. Never invoked at startup; retaining
// this vtable checks the real DNS/TLS backend without making any connection.
#if ORBIT_AI_TRANSPORT_CHECK
extern "C" void orbit_ai_link_probe(){
    // Even an accidental invocation uses disabled settings. Link-only: no boot
    // caller exists. Constructor + begin + step retain the complete adapter,
    // protocol and Engine, not merely the TLS class vtable.
    auto *channel=new(std::nothrow) Esp32Channel;if(!channel)return;
    auto *session=new(std::nothrow) ApiWorkerSession(*channel,ApiSettings{});
    if(session){Snapshot snapshot;capture(snapshot,Topic::Song,"Compile check","Synthetic artist","");session->begin(snapshot);session->step();delete session;}
    delete channel;
}
#endif
}
#else
namespace orbit_intro_ai {SecureChannel *createEsp32Channel(){return nullptr;}void destroyEsp32Channel(SecureChannel *p){delete p;}}
#endif
