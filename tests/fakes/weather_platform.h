#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <deque>
#include <string>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <ctime>
extern time_t fakeEpoch;
inline time_t weather_test_time(time_t *dest){if(dest)*dest=fakeEpoch;return fakeEpoch;}
#define time weather_test_time
#define PROGMEM
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4,WL_CONNECTED=3;
extern uint32_t ticks;extern size_t freeMemory,largestBlock;extern bool wifi;
extern int postCount,httpStatus,contentLength;extern bool transfer,stall,headerOverflow;
extern std::string responseBody,lastPayload;
extern bool allocationFailure;
inline void *heap_caps_malloc(size_t n,int){return allocationFailure?nullptr:malloc(n);}
inline void *heap_caps_realloc(void *p,size_t n,int){return allocationFailure?nullptr:realloc(p,n);}
struct FakeQueue{size_t size;std::string data;};
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(unsigned n,size_t size){assert(n==1);return new FakeQueue{size,{}};}
inline int xQueueOverwrite(QueueHandle_t q,const void *data){q->data.assign((const char*)data,q->size);return 1;}
inline int xQueuePeek(QueueHandle_t q,void *dest,unsigned wait){assert(wait==0);if(q->data.empty())return 0;memcpy(dest,q->data.data(),q->size);return 1;}
constexpr int pdTRUE=1;
inline uint32_t millis(){return ticks;}
inline uint32_t esp_random(){return 0x1256abcd;}
inline void vTaskDelay(unsigned n){ticks+=n;}
inline unsigned uxTaskGetStackHighWaterMark(void*){return 5000;}
inline size_t heap_caps_get_free_size(int){return freeMemory;}
inline size_t heap_caps_get_largest_free_block(int){return largestBlock;}
struct FakeSerial {
 std::deque<unsigned char> input;std::string output;
 int available(){return input.size();}int read(){if(input.empty())return -1;int c=input.front();input.pop_front();return c;}
 void println(const char *s){output+=std::string(s)+"\n";}
 void printf(const char *fmt,...){char out[1024];va_list args;va_start(args,fmt);vsnprintf(out,sizeof(out),fmt,args);va_end(args);output+=out;}
operator bool()const{return true;}
};extern FakeSerial Serial;
struct FakeWiFi{int status()const{return wifi?WL_CONNECTED:0;}};extern FakeWiFi WiFi;
class WiFiClientSecure {
public:
 bool stopped=false,ca=false,handshake=false;std::string bytes;size_t offset=0;
 virtual ~WiFiClientSecure()=default;
 void setCACert(const char *cert){ca=strstr(cert,"BEGIN CERTIFICATE");assert(ca);}
 void setHandshakeTimeout(unsigned n){handshake=n==4;assert(handshake);}
 virtual size_t write(uint8_t){return 1;}virtual size_t write(const uint8_t*,size_t n){return n;}
 virtual int available(){return !stopped&&!stall?int(bytes.size()-offset):0;}
 virtual uint8_t connected(){return !stopped&&(stall || offset<bytes.size());}
 virtual int read(){uint8_t b;return read(&b,1)>0?b:-1;}
 virtual int read(uint8_t*out,size_t n){if(stopped)return -1;n=std::min(n,bytes.size()-offset);memcpy(out,bytes.data()+offset,n);offset+=n;return n;}
 virtual void stop(){stopped=true;}
};
namespace fake_weather {inline void (*&request_hook())(){static void (*hook)()=nullptr;return hook;}}
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient {
 WiFiClientSecure *client=nullptr;bool timeout=false,connectTimeout=false,reuse=false,redirects=false,http10=false;
public:
 void setConnectTimeout(int n){connectTimeout=n==2000;assert(connectTimeout);}void setTimeout(int n){timeout=n==1500;assert(timeout);}
 void setReuse(bool v){reuse=!v;assert(reuse);}void setFollowRedirects(int n){redirects=n==0;assert(redirects);}void useHTTP10(bool v){http10=v;}
 bool begin(WiFiClientSecure &c,const char *url){assert(!strncmp(url,"https://ipwho.is/?fields=",23)||!strncmp(url,"https://api.open-meteo.com/v1/forecast?",38));lastPayload=url;client=&c;return true;}
 void collectHeaders(const char **h,unsigned n){assert(n==1&&!strcmp(h[0],"Transfer-Encoding"));}
 void addHeader(const char *name,const char *value){(void)name;(void)value;}
 int GET(){assert(client&&client->ca&&client->handshake&&timeout&&connectTimeout&&reuse&&redirects&&http10);++postCount;if(fake_weather::request_hook()){auto hook=fake_weather::request_hook();fake_weather::request_hook()=nullptr;hook();}uint8_t b=0;client->write(&b,1);ticks+=100;client->bytes=responseBody;
  if(headerOverflow){client->bytes=std::string(9000,'x');uint8_t out[8192];client->read(out,sizeof(out));client->read();}return httpStatus;}
 std::string header(const char*){return transfer?"chunked":"";}
 int getSize(){return contentLength==-2?int(responseBody.size()):contentLength;}
 WiFiClientSecure &getStream(){return *client;}bool connected(){return client->connected();}void end(){if(client)client->stop();}
};
