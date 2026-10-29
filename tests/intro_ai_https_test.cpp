#include "intro_ai_worker.h"
#include "intro_ai_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
using namespace orbit_intro_ai;
static unsigned assertions=0;
#define CHECK(v) do{++assertions;assert(v);}while(0)
static const char *Key="offline_fake_api_key_for_tests_only";
static const char *Ca="-----BEGIN CERTIFICATE-----\nSYNTHETIC\n-----END CERTIFICATE-----\n";
static ApiSettings settings(){ApiSettings c;c.enabled=true;c.key=Key;c.caPem=Ca;return c;}
struct FakeChannel:SecureChannel {
    uint64_t now=1000,utc=1793214000;
    bool wifi=true,clock=true,memory=true,openFailed=false,connection=true,stall=false,writeFail=false;
    unsigned opens=0,writes=0,closes=0,reads=0,openWait=0,zeroWrites=0;
    size_t offset=0,writeMax=512,readMax=512;
    uint64_t readCost=0,openCost=0,openDeadline=0;
    std::string response,sent;
    uint64_t nowMs()const override{return now;}
    uint64_t utcSeconds()const override{return utc;}
    Outcome readiness()const override{return !wifi?Outcome::Offline:!clock?Outcome::ClockUntrusted:!memory?Outcome::ResourceLimited:Outcome::Idle;}
    Open open(const ApiSettings &s,uint64_t deadline)override{openDeadline=deadline;CHECK(s.enabled&&!strcmp(s.caPem,Ca));++opens;now+=openCost;if(openWait){--openWait;return Open::Pending;}return openFailed?Open::Failed:Open::Ready;}
    Outcome failure()const override{return Outcome::TlsError;}
    int write(const uint8_t *p,size_t n)override{++writes;if(writeFail)return -1;if(zeroWrites){--zeroWrites;return 0;}n=std::min(n,writeMax);sent.append(reinterpret_cast<const char*>(p),n);return int(n);}
    int available()override{return stall?0:int(std::min(response.size()-offset,readMax));}
    int read(uint8_t *p,size_t n)override{++reads;now+=readCost;n=std::min(n,response.size()-offset);memcpy(p,response.data()+offset,n);offset+=n;return int(n);}
    bool connected()override{return connection&&(stall||offset<response.size());}
    void close()override{++closes;}
};
static std::string plain(const std::string &body,const std::string &headers="",int status=200){return "HTTP/1.1 "+std::to_string(status)+" Status\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+headers+"\r\n"+body;}
static std::string chunked(const std::string &body,size_t size){
    std::string out="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n";
    for(size_t i=0;i<body.size();i+=size){size_t n=std::min(size,body.size()-i);char hex[40];snprintf(hex,sizeof hex,"%x;fixture=1\r\n",unsigned(n));out+=hex+body.substr(i,n)+"\r\n";}
    return out+"0\r\nX-Fixture: local\r\n\r\n";
}
static Outcome parse(const std::string &wire,size_t split,std::string *output=nullptr,bool eof=true){
    HttpResponse p;std::vector<char> b(MaxResponseBytes);p.reset(b.data(),b.size(),1793214000);
    bool success=true;for(size_t i=0;i<wire.size()&&!p.done()&&success;i+=split)success=p.feed(reinterpret_cast<const uint8_t*>(wire.data()+i),std::min(split,wire.size()-i));
    if(eof)p.eof();if(output)*output=std::string(b.data(),p.result().bytes);
    return p.error()!=Outcome::Idle?p.error():p.done()?Outcome::Ready:Outcome::Pending;
}
static bool holdsSecret(const HttpsTransport &t){const auto *p=reinterpret_cast<const unsigned char*>(&t);return std::search(p,p+sizeof t,Key,Key+strlen(Key))!=p+sizeof t;}
static Poll finish(HttpsTransport &t,FakeChannel &c,char *body,HttpResult &reply){Poll r=Poll::Pending;for(unsigned i=0;i<5000&&r==Poll::Pending;++i){r=t.poll(body,MaxResponseBytes,reply);c.now+=5;}return r;}
int main(int argc,char **argv){
    CHECK(argc==2);std::ifstream f(argv[1]);const std::string body((std::istreambuf_iterator<char>(f)),{});CHECK(!body.empty());
    CHECK(!configuredApiSettings().enabled);auto cfg=settings();CHECK(validateSettings(cfg)==SettingsStatus::Ready);
    cfg.enabled=false;cfg.key="\r\nnot read";CHECK(validateSettings(cfg)==SettingsStatus::Disabled);
    for(const char *key:{"","short","fake_key_with_\r\nHost: evil.example","contains space in key"}){cfg=settings();cfg.key=key;CHECK(validateSettings(cfg)!=SettingsStatus::Ready);}
    std::string longKey(513,'a');cfg=settings();cfg.key=longKey.c_str();CHECK(validateSettings(cfg)==SettingsStatus::InvalidKey);
    for(const char *ca:{"","no certificate","-----BEGIN CERTIFICATE-----\nno end","-----BEGIN CERTIFICATE-----\t-----END CERTIFICATE-----"}){cfg=settings();cfg.caPem=ca;CHECK(validateSettings(cfg)!=SettingsStatus::Ready);}
    std::string oversizedCa(16385,'a');cfg=settings();cfg.caPem=oversizedCa.c_str();CHECK(validateSettings(cfg)==SettingsStatus::InvalidCa);
    for(size_t split:{1u,2u,7u,31u,512u,8192u}){
        std::string decoded;CHECK(parse(plain(body),split,&decoded)==Outcome::Ready&&decoded==body);
        CHECK(parse(chunked(body,37),split,&decoded)==Outcome::Ready&&decoded==body);
        CHECK(parse("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n"+body,split,&decoded)==Outcome::Ready&&decoded==body);
    }
    for(const std::string &wire:{
        "HTTP/1.1 200 OK\n\n"+body,
        plain(body,"Content-Length: 1\r\n"),plain(body,"Transfer-Encoding: chunked\r\n"),
        plain(body,"Content-Encoding: gzip\r\n"),plain(body,"Retry-After: 1\r\nRetry-After: 2\r\n"),
        std::string("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n")+body,
        std::string("HTTP/1.1 200 OK\r\nContent-Length: -1\r\nContent-Type: application/json\r\n\r\n")+body,
        std::string("HTTP/1.1 200 OK\r\nContent-Length: 9999999999999999999999999999999\r\n\r\n"),
        std::string("HTTP/1.1 100 Continue\r\n\r\n"),
        std::string("HTTP/1.1 200 OK\r\nBad Header: value\r\n\r\n"),
        std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: gzip, chunked\r\n\r\n")})CHECK(parse(wire,3)==Outcome::ProtocolError);
    CHECK(parse(plain(std::string(MaxResponseBytes+1,'x')),512)==Outcome::TooLarge);
    CHECK(parse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"+std::string(MaxResponseBytes+1,'x'),512)==Outcome::TooLarge);
    CHECK(parse(plain(body).substr(0,plain(body).size()-2),512)==Outcome::Incomplete);
    CHECK(parse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nX: "+std::string(1024,'x')+"\r\n\r\n",512)==Outcome::ProtocolError);
    std::string many="HTTP/1.1 200 OK\r\n";for(int i=0;i<500;++i)many+="X-Fixture: value\r\n";
    CHECK(parse(many,512)==Outcome::TooLarge);
    for(const char *tail:{"g\r\n","1\r\na!\r\n0\r\n\r\n","0\r\nContent-Length: 2\r\n\r\n"})CHECK(parse(std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n")+tail,1)==Outcome::ProtocolError);
    // Every prefix of a length-delimited response fails closed until complete.
    const auto wire=plain(body);for(size_t n=0;n<wire.size();n+=13)CHECK(parse(wire.substr(0,n),11)!=Outcome::Ready);
    for(int status:{301,302,307,400,401,403,429,500,503}){
        std::vector<char> out(MaxResponseBytes);HttpResponse p;p.reset(out.data(),out.size());
        const auto w=plain("PRIVATE ERROR BODY", "Location: https://evil.example/\r\nRetry-After: 123\r\nContent-Encoding: gzip\r\n",status);
        CHECK(p.feed(reinterpret_cast<const uint8_t*>(w.data()),w.size())&&p.done()&&p.result().status==status&&p.result().bytes==0&&p.result().retryAfterSeconds==123);
    }
    // Retry-After HTTP dates use trusted UTC and validate real calendar dates.
    for(const auto &item:std::vector<std::pair<std::string,uint32_t>>{{"3600",3600},{"429496729500000000000",UINT32_MAX},{"not-a-date",0},{"Wed, 28 Oct 2026 19:00:00 GMT",7200},{"Fri, 30 Feb 2026 19:00:00 GMT",0}}){
        char b[32];HttpResponse p;p.reset(b,sizeof b,1793206800);const auto w=plain("","Retry-After: "+item.first+"\r\n",429);p.feed(reinterpret_cast<const uint8_t*>(w.data()),w.size());CHECK(p.done()&&p.result().retryAfterSeconds==item.second);
    }
    std::vector<char> buffer(MaxResponseBytes);
    // Disabled/misconfigured/offline/unsynced/low-memory never open DNS/TLS.
    for(unsigned kind=0;kind<6;++kind){FakeChannel c;auto s=settings();if(kind==0)s.enabled=false;if(kind==1)s.key="";if(kind==2)s.caPem="";if(kind==3)c.wifi=false;if(kind==4)c.clock=false;if(kind==5)c.memory=false;HttpsTransport t(c,s);CHECK(!t.start(Endpoint,"{}",2,1,DeadlineMs));CHECK(c.opens==0&&c.writes==0&&!holdsSecret(t));}
    {FakeChannel c;HttpsTransport t(c,settings());CHECK(!t.start("http://api.openai.com/v1/responses","{}",2,1,DeadlineMs));CHECK(!t.start("https://evil.example/v1/responses","{}",2,1,DeadlineMs));CHECK(!c.opens);}
    for(bool chunks:{false,true}){
        FakeChannel c;c.response=chunks?chunked(body,29):plain(body);c.writeMax=17;c.readMax=19;c.zeroWrites=2;
        HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{\"fixture\":true}",16,88,DeadlineMs));CHECK(holdsSecret(t));
        CHECK(finish(t,c,buffer.data(),reply)==Poll::Complete&&reply.status==200&&reply.generation==88&&reply.bytes==body.size());
        CHECK(std::string(buffer.data(),reply.bytes)==body&&!holdsSecret(t));
        CHECK(c.sent.find("POST /v1/responses HTTP/1.1\r\nHost: api.openai.com\r\n") ==0);
        CHECK(c.sent.find(std::string("Authorization: Bearer ")+Key+"\r\n")!=std::string::npos);
        CHECK(c.sent.find("Connection: close\r\n")!=std::string::npos&&c.sent.find("Accept-Encoding: identity\r\n")!=std::string::npos);
        CHECK(c.sent.substr(c.sent.find("\r\n\r\n")+4)=="{\"fixture\":true}"&&c.opens==1);
    }
    {FakeChannel c;c.openFailed=true;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::TlsError&&!holdsSecret(t));CHECK(c.writes==0);}
    {FakeChannel c;c.response=plain(body);HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));t.requestCancel();CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Canceled&&c.opens==0&&!holdsSecret(t));}
    {FakeChannel c;c.openCost=DeadlineMs;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout&&!c.writes);}
    {FakeChannel c;c.zeroWrites=100000;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));CHECK(finish(t,c,buffer.data(),reply)==Poll::Pending);c.now=31000;CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout&&!holdsSecret(t));CHECK(c.opens==1);}
    {FakeChannel c;c.stall=true;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));for(unsigned i=0;i<3;++i)t.poll(buffer.data(),buffer.size(),reply);c.now+=DeadlineMs;CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout);}
    {FakeChannel c;c.response=plain(body);c.readMax=1;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));t.poll(buffer.data(),buffer.size(),reply);t.poll(buffer.data(),buffer.size(),reply);c.now+=2001;CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout);}
    {FakeChannel c;c.response=plain(body);c.readCost=DeadlineMs;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));CHECK(finish(t,c,buffer.data(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout);}
    {FakeChannel c;c.response=plain(body);HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));c.wifi=false;CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Offline&&c.opens==0);}
    // Same Engine and worker connection point: native request -> HTTPS bytes ->
    // native fixture parse -> Ready -> cache, with no real API/client involved.
    {FakeChannel c;c.response=plain(body);std::unique_ptr<ApiWorkerSession> w(new ApiWorkerSession(c,settings()));Snapshot s;CHECK(capture(s,Topic::Song,"Example Song","Example Artist","Example Release",250));
     CHECK(w->begin(s)==Outcome::Pending);for(unsigned i=0;i<100&&w->engine().outcome()==Outcome::Pending;++i){w->step();c.now+=10;}
     CHECK(w->engine().outcome()==Outcome::Ready&&w->engine().result());CHECK(c.sent.find(Key)!=std::string::npos);CHECK(c.sent.substr(c.sent.find("\r\n\r\n")+4).find(Key)==std::string::npos);
     const unsigned count=c.opens;CHECK(w->begin(s)==Outcome::Ready&&w->engine().fromCache()&&c.opens==count);
     for(Topic topic:{Topic::Artist,Topic::Release}){c.now+=2000;c.offset=0;s.topic=topic;CHECK(w->begin(s)==Outcome::Pending);for(unsigned i=0;i<100&&w->engine().outcome()==Outcome::Pending;++i){w->step();c.now+=10;}CHECK(w->engine().outcome()==Outcome::Ready);w->cancel();}
     const unsigned tourCount=c.opens;
     for(Topic topic:{Topic::Artist,Topic::Release,Topic::Song}){s.topic=topic;CHECK(w->begin(s)==Outcome::Ready&&w->engine().fromCache()&&c.opens==tourCount);w->cancel();}
     w->trackChanged();c.now+=2000;CHECK(w->begin(s)==Outcome::Pending&&!w->engine().fromCache());w->requestCancel();CHECK(w->step()==Outcome::Canceled);
    }
    {FakeChannel c;c.response=plain("echo omitted","",401);std::unique_ptr<ApiWorkerSession> w(new ApiWorkerSession(c,settings()));Snapshot s;CHECK(capture(s,Topic::Artist,"","Example Artist",""));CHECK(w->begin(s)==Outcome::Pending);for(unsigned i=0;i<50&&w->engine().outcome()==Outcome::Pending;++i){w->step();c.now+=10;}CHECK(w->engine().outcome()==Outcome::ConfigError);c.now+=100000;CHECK(w->begin(s)==Outcome::ConfigError&&c.opens==1);}
    {FakeChannel c;c.stall=true;std::unique_ptr<ApiWorkerSession> w(new ApiWorkerSession(c,settings()));Snapshot s;CHECK(capture(s,Topic::Song,"Example Song","Example Artist",""));CHECK(w->begin(s)==Outcome::Pending);w->requestCancel();CHECK(w->step()==Outcome::Canceled&&!w->engine().result()&&c.opens==0);}
    // Absolute UI deadline survives queue waiting and time spent constructing JSON.
    {FakeChannel c;HttpsTransport t(c,settings());HttpResult reply;t.limitDeadline(c.now+4000);c.now+=1000;c.openCost=3500;
     CHECK(t.start(Endpoint,"{}",2,1,DeadlineMs));CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout);
     CHECK(c.openDeadline==5000&&c.writes==0&&!holdsSecret(t));}
    {FakeChannel c;HttpsTransport t(c,settings());t.limitDeadline(c.now);CHECK(!t.start(Endpoint,"{}",2,1,DeadlineMs)&&t.startFailure()==Outcome::Timeout&&c.opens==0&&!holdsSecret(t));
     t.cancel();c.response=plain(body);CHECK(t.start(Endpoint,"{}",2,2,DeadlineMs));HttpResult reply;CHECK(finish(t,c,buffer.data(),reply)==Poll::Complete);}
    // Slow body with a valid header is bounded by the independent5s idle limit.
    {FakeChannel c;c.response="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n";HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));for(unsigned i=0;i<2;++i)t.poll(buffer.data(),buffer.size(),reply);c.stall=true;c.now+=5000;CHECK(t.poll(buffer.data(),buffer.size(),reply)==Poll::Failed&&reply.failure==Outcome::Timeout);}
    {FakeChannel c;c.stall=true;HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));t.poll(buffer.data(),buffer.size(),reply);t.poll(buffer.data(),buffer.size(),reply);std::vector<char> other(MaxResponseBytes);CHECK(t.poll(other.data(),other.size(),reply)==Poll::Failed&&reply.failure==Outcome::ProtocolError);}
    {FakeChannel c;c.response=plain(Key,"Retry-After: 5\r\n",429);HttpsTransport t(c,settings());HttpResult reply;CHECK(t.start(Endpoint,"{}",2,3,DeadlineMs));CHECK(finish(t,c,buffer.data(),reply)==Poll::Complete&&reply.status==429&&reply.bytes==0&&!holdsSecret(t));}
    printf("PASS: %u HTTPS/config/framing/Engine assertions, ASan/UBSan; parser=%zu transport=%zu worker-session=%zu bytes (host ABI); synthetic TLS only, no sockets/real API/device calls.\n",assertions,sizeof(HttpResponse),sizeof(HttpsTransport),sizeof(ApiWorkerSession));
}
