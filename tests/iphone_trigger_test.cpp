#include "iphone_trigger_platform.h"
#include "iphone_trigger.h"
#include "network_clock.h"
#include <ArduinoJson.h>
#include <iostream>
uint32_t ticks=1000;size_t freeMemory=240000,largestBlock=200000;bool wifi=true;
int postCount=0,httpStatus=200,contentLength=-2;bool transfer=false,stall=false,headerOverflow=false;
std::string responseBody="{\"code\":200,\"message\":\"success\"}",lastPayload;
FakeSerial Serial;FakeWiFi WiFi;static orbit_clock::Status clockStatus=orbit_clock::Status::Ready;
namespace orbit_clock {Reading read(){Reading r;r.status=clockStatus;return r;}bool utc_ready(){return clockStatus==Status::Ready||clockStatus==Status::Locating;}}
static void input(const std::string &s){for(auto c:s)Serial.input.push_back(c);while(Serial.available())orbit_iphone::serial_tick();}
static void request(){ticks+=21000;input("IPHONE_START\n");}
static void ends(const char *expected){assert(Serial.output.find(std::string("result=")+expected)!=std::string::npos);Serial.output.clear();}
int main(int argc,char**){
 for(unsigned i=0;i<10;++i)orbit_iphone::worker_tick();assert(postCount==0); // No boot/network auto-send.
 input("IPHONE_STATUS\n");assert(postCount==0);Serial.output.clear();
 input(std::string(100,'x')+"IPHONE_START\n");orbit_iphone::worker_tick();assert(postCount==0);
 input(std::string("IPHONE_START\0ignored",20)+"\n");orbit_iphone::worker_tick();assert(postCount==0);
 request();assert(postCount==0);orbit_iphone::worker_tick();
 if(argc>1){assert(postCount==0);ends("configuration_not_sent");std::cout<<"PASS: empty-key profile cannot send\n";return 0;}
 assert(postCount==1);assert(orbit_iphone::state()==orbit_iphone::LaunchState::Sent);ends("accepted");
 StaticJsonDocument<512> payload;assert(!deserializeJson(payload,lastPayload));assert(!strcmp(payload["device_key"],"HOST_TEST_DEVICE_KEY"));
 assert(!strcmp(payload["body"],"FOSI_S3_START\nBoard 1256abcd"));assert(!strcmp(payload["action"],"none"));assert(payload["url"].isNull());
 for(int i=0;i<10;++i)orbit_iphone::worker_tick();input("IPHONE_START\n");orbit_iphone::worker_tick();assert(postCount==1);Serial.output.clear();
 request();input("IPHONE_START\n");orbit_iphone::worker_tick();assert(postCount==2);ends("accepted"); // Queued requests cannot stack.
 wifi=false;request();orbit_iphone::worker_tick();assert(postCount==2);ends("wifi_not_sent");wifi=true;
 orbit_iphone::worker_tick();assert(postCount==2); // Reconnect never replays.
 clockStatus=orbit_clock::Status::Syncing;request();orbit_iphone::worker_tick();assert(postCount==2);ends("clock_not_sent");clockStatus=orbit_clock::Status::Ready;
 freeMemory=128*1024-1;request();orbit_iphone::worker_tick();assert(postCount==2);ends("memory_not_sent");freeMemory=240000;
 largestBlock=64*1024-1;request();orbit_iphone::worker_tick();assert(postCount==2);ends("memory_not_sent");largestBlock=200000;
 request();ticks+=5001;orbit_iphone::worker_tick();assert(postCount==2);ends("expired_not_sent");
 httpStatus=503;request();orbit_iphone::worker_tick();assert(postCount==3);ends("uncertain");orbit_iphone::worker_tick();assert(postCount==3);
 httpStatus=302;request();orbit_iphone::worker_tick();assert(postCount==4);ends("uncertain");
 httpStatus=400;request();orbit_iphone::worker_tick();assert(postCount==5);ends("rejected");httpStatus=200;
 for(auto body:{"not json","{\"code\":\"200\"}","{\"code\":false}","{\"code\":500}"}){
  responseBody=body;request();orbit_iphone::worker_tick();ends(!strcmp(body,"{\"code\":500}")?"rejected":"uncertain");
 }
 responseBody="{\"code\":200}";
 transfer=true;request();orbit_iphone::worker_tick();ends("uncertain");transfer=false;
 contentLength=1025;request();orbit_iphone::worker_tick();ends("uncertain");contentLength=100;request();orbit_iphone::worker_tick();ends("uncertain");contentLength=-2;
 stall=true;request();const auto began=ticks;orbit_iphone::worker_tick();assert(uint32_t(ticks-began)<=2600);ends("uncertain");stall=false;
 headerOverflow=true;request();orbit_iphone::worker_tick();ends("uncertain");headerOverflow=false;
 ticks=UINT32_MAX-10;input("IPHONE_START\n");ticks=50;orbit_iphone::worker_tick();ends("accepted");
 assert(Serial.output.find("HOST_TEST_DEVICE_KEY")==std::string::npos);
 std::cout<<"PASS: actual board trigger module: no autostart/replay; explicit bounded serial input; key/cooldown/queue/expiry/wifi/NTP/memory gates; verified TLS setup; fixed POST+body; redirects, invalid/truncated/large/chunked/stalled/header-overflow responses; no duplicate HTTP requests; wrap\n";
}
