#pragma once
// Host-only substitute for EVERY platform, DNS and TLS symbol in the backend.
// It contains no sockets, networking APIs or credentials.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <assert.h>
#include <time.h>
namespace fake_ai {
extern uint64_t ms;extern bool wifi,utcSynced,oom;
extern size_t freeBytes,largest;
extern unsigned dnsCalls,tlsCalls,tlsInit,tlsFree;
extern int dnsMode,tlsResult,verifyFlags,wantWrites;
extern std::string incoming,outgoing;extern size_t offset;
extern void (*dnsCallback)(const char *,const struct FakeIp *,void *);
extern void *dnsArg;
extern void (*connectHook)();
struct FakeIp {uint32_t value;};
}
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,WL_CONNECTED=3;
inline size_t heap_caps_get_free_size(int caps){assert(caps==3);return fake_ai::freeBytes;}
inline size_t heap_caps_get_largest_free_block(int caps){assert(caps==3);return fake_ai::largest;}
inline void *heap_caps_malloc(size_t bytes,int caps){assert(caps==3);return fake_ai::oom?nullptr:malloc(bytes);}
inline int64_t esp_timer_get_time(){return int64_t(fake_ai::ms*1000);}
inline time_t ai_fake_time(time_t*){return 1793214000;}
#define time ai_fake_time
struct FakeWiFi {int status()const{return fake_ai::wifi?WL_CONNECTED:0;}};
static FakeWiFi WiFi __attribute__((unused));
namespace orbit_clock {inline bool utc_ready(){return fake_ai::utcSynced;}}
class IPAddress {public:uint32_t value;explicit IPAddress(uint32_t address):value(address){}};
using ip_addr_t=fake_ai::FakeIp;
inline bool IP_IS_V4(const ip_addr_t*){return true;}
inline const ip_addr_t *ip_2_ip4(const ip_addr_t *p){return p;}
inline uint32_t ip4_addr_get_u32(const ip_addr_t *p){return p->value;}
constexpr int ERR_OK=0,ERR_INPROGRESS=-5;
inline int tcpip_callback(void (*f)(void*),void *arg){f(arg);return ERR_OK;}
inline int dns_gethostbyname(const char *host,ip_addr_t *out,void (*callback)(const char*,const ip_addr_t*,void*),void *arg){
    assert(!strcmp(host,"api.openai.com"));++fake_ai::dnsCalls;
    if(fake_ai::dnsMode==0){out->value=0x01020304;return ERR_OK;}
    fake_ai::dnsCallback=callback;fake_ai::dnsArg=arg;return ERR_INPROGRESS;
}
constexpr int MBEDTLS_ERR_SSL_WANT_READ=-1,MBEDTLS_ERR_SSL_WANT_WRITE=-2,MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY=-3;
struct FakeSsl {};
struct sslclient_context {int socket;unsigned handshake_timeout;FakeSsl ssl_ctx;};
inline void ssl_init(sslclient_context *c){++fake_ai::tlsInit;c->socket=-1;}
inline int start_ssl_client(sslclient_context *ctx,const IPAddress &ip,uint32_t port,const char *host,int timeout,const char *ca,bool bundle,const char *cert,const char *key,const char *pskId,const char *psk,bool insecure,const char **alpn){
    assert(ip.value==0x01020304&&port==443&&!strcmp(host,"api.openai.com")&&timeout==2000&&ctx->handshake_timeout==4000);
    assert(ca&&strstr(ca,"BEGIN CERTIFICATE")&&!bundle&&!cert&&!key&&!pskId&&!psk&&!insecure&&!alpn);
    ++fake_ai::tlsCalls;if(fake_ai::connectHook)fake_ai::connectHook();ctx->socket=fake_ai::tlsResult;return fake_ai::tlsResult;
}
inline void stop_ssl_socket(sslclient_context *,const char*,const char*,const char*){++fake_ai::tlsFree;}
inline unsigned mbedtls_ssl_get_verify_result(FakeSsl*){return unsigned(fake_ai::verifyFlags);}
inline int mbedtls_ssl_write(FakeSsl*,const uint8_t *p,size_t n){if(fake_ai::wantWrites){--fake_ai::wantWrites;return MBEDTLS_ERR_SSL_WANT_WRITE;}fake_ai::outgoing.append(reinterpret_cast<const char*>(p),n);return int(n);}
inline int mbedtls_ssl_read(FakeSsl*,uint8_t *p,size_t n){
    if(fake_ai::offset>=fake_ai::incoming.size())return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    n=std::min(n,fake_ai::incoming.size()-fake_ai::offset);memcpy(p,fake_ai::incoming.data()+fake_ai::offset,n);fake_ai::offset+=n;return int(n);
}
