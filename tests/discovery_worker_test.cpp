#include "s3_protocol.h"
#include "discovery_policy.h"
#include <assert.h>
#include <string>
#include <iostream>
using String=std::string;
struct IPAddress {
    uint32_t raw;explicit IPAddress(uint32_t r):raw(r){}
    uint8_t operator[](int i)const{return uint8_t(raw>>(8*i));}
    String toString()const{char s[16];snprintf(s,sizeof(s),"%u.%u.%u.%u",(*this)[0],(*this)[1],(*this)[2],(*this)[3]);return s;}
};
struct Wifi {IPAddress localIP(){return IPAddress(0x500200c0);}IPAddress subnetMask(){return IPAddress(0x00ffffff);}} WiFi;
struct mdns_txt_item_t{const char *key,*value;};
struct mdns_ip_addr_t{struct{int type;struct{struct{uint32_t addr;}ip4;}u_addr;}addr;mdns_ip_addr_t *next;};
struct mdns_result_t{mdns_result_t *next;mdns_txt_item_t *txt;size_t txt_count;mdns_ip_addr_t *addr;};
using esp_err_t=int;constexpr int ESP_OK=0,MDNS_IP_PROTOCOL_V4=0;
static mdns_result_t *answer=nullptr;static unsigned freed=0,reads=0;
static int scenario=0;
esp_err_t mdns_query_ptr(const char *service,const char *proto,uint32_t timeout,size_t maximum,mdns_result_t **out){
    assert(!strcmp(service,"_googlecast")&&!strcmp(proto,"_tcp")&&timeout==1200&&maximum==8);*out=answer;return ESP_OK;
}
void mdns_query_results_free(mdns_result_t*){++freed;}
namespace s3 {
String apiBase;char apiIp[16]={};
enum class Result{Ok,Missing,Error,Transport};
String escape(const char *s){return s;}
Result request(const String &url,JsonDocument &reply){
    ++reads;assert(url.find("/api/getData?path=")==0 && url.find("&roles=value")!=String::npos);
    assert(apiBase=="http://192.0.2.23");
    const char *json=url.find("lastAudioSource")!=String::npos?
        (scenario==2?"[{\"type\":\"i32_\",\"i32_\":99}]":"[{\"type\":\"i32_\",\"i32_\":0}]"):
        (scenario==1?"[{\"type\":\"string_\",\"string_\":\"Other device\"}]":"[{\"type\":\"string_\",\"string_\":\"Fosi S3\"}]");
    assert(!deserializeJson(reply,json));return Result::Ok;
}
// ACTUAL_DISCOVERY
}
int main(){
    using namespace s3;
    const char *identity="00112233445566778899aabbccddeeff";
    mdns_txt_item_t txt[]={{"md","S3"},{"id",identity}};
    mdns_ip_addr_t ip={{0,{{0x170200c0}}},nullptr};
    mdns_result_t first={nullptr,txt,2,&ip};answer=&first;
    DynamicJsonDocument doc(4096);char pinned[65]={},message[80]={};
    assert(discover(doc,pinned,message));assert(!strcmp(pinned,identity)&&apiBase=="http://192.0.2.23"&&reads==2&&freed==1);
    for(int failure:{1,2}){
        pinned[0]=0;scenario=failure;reads=freed=0;
        assert(!discover(doc,pinned,message));assert(apiBase.empty()&&!apiIp[0]&&!pinned[0]&&freed==1);
    }
    scenario=0;reads=freed=0;
    mdns_txt_item_t otherTxt[]={{"md","S3"},{"id","11112233445566778899aabbccddeeff"}};
    mdns_result_t other={nullptr,otherTxt,2,&ip};first.next=&other;
    assert(!discover(doc,pinned,message)&&reads==0&&freed==1); // No arbitrary device selection.
    first.next=nullptr;answer=&other;snprintf(pinned,sizeof(pinned),"%s",identity);reads=0;
    assert(!discover(doc,pinned,message)&&reads==0); // Never replace the pinned identity.
    answer=nullptr;reads=0;assert(!discover(doc,pinned,message)&&reads==0);
    answer=&first;ip.addr.u_addr.ip4.addr=0x1702a8c0;reads=0;
    assert(!discover(doc,pinned,message)&&reads==0); // Out-of-subnet advertisements are not contacted.
    std::cout<<"PASS: actual discovery orchestration; read-only model/source verification, multiple devices, pinning and rejected LAN targets\n";
}
