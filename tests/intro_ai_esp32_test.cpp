#include "intro_ai_esp32.h"
#include "ai_platform_fake.h"
#include <stdio.h>
#include <memory>
#include <vector>
#include <fstream>
#include <iterator>
namespace fake_ai {
uint64_t ms=100;bool wifi=true,utcSynced=true,oom=false;
size_t freeBytes=256*1024,largest=128*1024;
unsigned dnsCalls=0,tlsCalls=0,tlsInit=0,tlsFree=0;
int dnsMode=0,tlsResult=42,verifyFlags=0,wantWrites=0;
std::string incoming,outgoing;size_t offset=0;
void (*dnsCallback)(const char *,const FakeIp *,void *)=nullptr;void *dnsArg=nullptr;void (*connectHook)()=nullptr;
}
using namespace orbit_intro_ai;
static HttpsTransport *cancelTarget=nullptr;
static void cancelWhileConnecting(){assert(cancelTarget);cancelTarget->requestCancel();}
static ApiSettings settings(){ApiSettings s;s.enabled=true;s.key="fake_api_key_for_offline_tests";s.caPem="-----BEGIN CERTIFICATE-----\nFAKE\n-----END CERTIFICATE-----\n";return s;}
static void reset(){using namespace fake_ai;wifi=utcSynced=true;oom=false;freeBytes=256*1024;largest=128*1024;dnsMode=0;tlsResult=42;verifyFlags=wantWrites=0;incoming=outgoing="";offset=0;connectHook=nullptr;}
int main(int argc,char **argv){
    assert(argc==2);std::ifstream file(argv[1]);const std::string body((std::istreambuf_iterator<char>(file)),{});
    // Even this host fake build never reads a private file or enables config.
    assert(!configuredApiSettings().enabled);
    auto c=createEsp32Channel();assert(c&&fake_ai::tlsInit==0&&fake_ai::dnsCalls==0);assert(createEsp32Channel()==nullptr);
    fake_ai::wifi=false;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::Offline&&fake_ai::dnsCalls==0);
    fake_ai::wifi=true;fake_ai::utcSynced=false;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::ClockUntrusted&&fake_ai::dnsCalls==0);
    fake_ai::utcSynced=true;fake_ai::freeBytes=100000;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::ResourceLimited&&fake_ai::dnsCalls==0);
    reset();fake_ai::largest=10000;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::ResourceLimited&&fake_ai::dnsCalls==0);
    reset();fake_ai::oom=true;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::ResourceLimited&&fake_ai::tlsInit==0);
    reset();fake_ai::verifyFlags=1;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::TlsError&&fake_ai::tlsInit==fake_ai::tlsFree);
    reset();fake_ai::tlsResult=-1;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::TlsError&&fake_ai::tlsInit==fake_ai::tlsFree);
    reset();assert(c->open(settings(),fake_ai::ms+6999)==Open::Failed&&c->failure()==Outcome::Timeout);
    reset();assert(c->open(settings(),fake_ai::ms+30000)==Open::Ready);
    fake_ai::wantWrites=1;assert(c->write(reinterpret_cast<const uint8_t*>("x"),1)==0);assert(c->write(reinterpret_cast<const uint8_t*>("x"),1)==1);
    fake_ai::incoming="hello";uint8_t buffer[20];assert(c->available()==5&&c->read(buffer,2)==2&&c->available()==3&&c->read(buffer,20)==3);assert(c->available()==0&&!c->connected());c->close();assert(fake_ai::tlsInit==fake_ai::tlsFree);
    // Cancel/free the channel before DNS completion. The real backend callback
    // still writes only to its static slot; ASan verifies no freed object access.
    reset();fake_ai::dnsMode=1;assert(c->open(settings(),fake_ai::ms+30000)==Open::Pending);
    fake_ai::ms+=3001;assert(c->open(settings(),fake_ai::ms+30000)==Open::Failed&&c->failure()==Outcome::Timeout);destroyEsp32Channel(c);
    assert(fake_ai::dnsCallback);fake_ai::FakeIp address{0x01020304};fake_ai::dnsCallback(nullptr,&address,fake_ai::dnsArg);
    reset();c=createEsp32Channel();auto calls=fake_ai::dnsCalls;assert(c->open(settings(),fake_ai::ms+30000)==Open::Ready&&fake_ai::dnsCalls==calls);c->close();
    // Actual backend (fake platform) -> actual HTTPS framing -> actual Engine.
    reset();fake_ai::incoming="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    {
        HttpsTransport transport(*c,settings());std::vector<char> response(MaxResponseBytes);HttpResult reply;
        assert(transport.start(Endpoint,"{}",2,7,DeadlineMs));Poll state=Poll::Pending;
        for(unsigned i=0;i<200&&state==Poll::Pending;++i){state=transport.poll(response.data(),response.size(),reply);fake_ai::ms+=10;}
        assert(state==Poll::Complete&&reply.status==200&&reply.bytes==body.size()&&reply.generation==7);
        assert(std::string(response.data(),reply.bytes)==body);assert(fake_ai::outgoing.find("Host: api.openai.com\r\nAuthorization: Bearer fake_api_key_for_offline_tests\r\n")!=std::string::npos);
    }
    reset();
    {HttpsTransport transport(*c,settings());std::vector<char> response(MaxResponseBytes);HttpResult reply;cancelTarget=&transport;fake_ai::connectHook=cancelWhileConnecting;
     assert(transport.start(Endpoint,"{}",2,9,DeadlineMs));assert(transport.poll(response.data(),response.size(),reply)==Poll::Failed&&reply.failure==Outcome::Canceled&&fake_ai::outgoing.empty());cancelTarget=nullptr;fake_ai::connectHook=nullptr;}
    assert(fake_ai::tlsInit==fake_ai::tlsFree);destroyEsp32Channel(c);
    puts("PASS: actual ESP32 backend compiled with fake DNS/TLS/heap/clock only; disabled configuration, Wi-Fi/time/memory/OOM rejection, CA+hostname+port+VERIFY checks, nonblocking read/write, bounded DNS/late callback, cleanup and real HTTPS framing; NO sockets/API/device operations");
}
