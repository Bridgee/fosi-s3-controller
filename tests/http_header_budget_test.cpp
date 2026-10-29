#include "http_header_budget.h"
#include <assert.h>
#include <string.h>
#include <string>
#include <iostream>

struct Clock {
    uint32_t ticks=0;
    uint32_t now() const {return ticks;}
    void idle() {++ticks;}
};
struct Transport {
    std::string bytes;
    size_t offset=0;
    bool stopped=false;
    virtual ~Transport()=default;
    virtual size_t write(uint8_t) {return 1;}
    virtual size_t write(const uint8_t *,size_t n) {return n;}
    virtual int available() {return stopped?0:int(bytes.size()-offset);}
    virtual uint8_t connected() {return !stopped;}
    virtual void stop() {stopped=true;}
    virtual int read() {uint8_t b;return read(&b,1)==1?b:-1;}
    virtual int read(uint8_t *out,size_t n) {
        if(stopped) return -1;
        if(n>bytes.size()-offset) n=bytes.size()-offset;
        memcpy(out,bytes.data()+offset,n);offset+=n;return int(n);
    }
};
using Client=s3::HeaderBudgetClient<Transport,Clock>;
int main() {
    uint8_t buffer[9000]={};
    {
        Clock c;Client client(c);client.bytes=std::string(20000,'x');
        client.armHeaders();c.ticks=100000; // Connecting must not consume the HTTP deadline.
        assert(client.connected());assert(client.write(buffer,1)==1);
        assert(client.read(buffer,sizeof(buffer))==8192);
        assert(client.read()==-1 && client.headerLimitHit() && client.stopped);
        client.finishHeaders();assert(!client.connected()); // Cannot accept a partial successful status.
    }
    {
        Clock c;Client client(c);client.bytes="HTTP/1.0 200 OK\r\n\r\n";
        client.armHeaders();assert(client.write(uint8_t('G'))==1);
        c.ticks=1999;assert(client.read()=='H');
        c.ticks=2000;assert(client.available()==0 && !client.connected());
        assert(client.headerLimitHit());
    }
    {
        Clock c;Client client(c);client.bytes=std::string(20000,'b');
        client.armHeaders();client.write(buffer,1);assert(client.read()=='b');
        client.finishHeaders();c.ticks=100000;
        assert(client.read(buffer,sizeof(buffer))==9000 && !client.headerLimitHit());
    }
    {
        Clock c;c.ticks=UINT32_MAX-1000;Client client(c);
        client.armHeaders();client.write(buffer,1);c.ticks=998;
        assert(client.connected());c.ticks=999;assert(!client.connected());
    }
    {
        Clock c;Client client(c);client.armHeaders();client.write(buffer,1);
        assert(client.read()==-1 && c.ticks==1); // Yield while Stream waits for more bytes.
        c.ticks=2000;assert(client.read()==-1 && c.ticks==2001);
    }
    std::cout<<"PASS: HTTP header byte/deadline limits, handshake exclusion, body phase, clock wrap and idle yield\n";
}
