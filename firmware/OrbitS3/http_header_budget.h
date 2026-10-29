#pragma once
#include <stddef.h>
#include <stdint.h>

namespace s3 {
// HTTPClient uses Stream::readStringUntil for headers: its per-byte timeout
// alone does not bound either a long line or a slowly trickling response.
// This adapter bounds that phase without replacing the SDK HTTP/TLS parser.
template<class Transport,class Clock> class HeaderBudgetClient : public Transport {
    Clock &clock;
    size_t consumed=0;
    uint32_t started=0;
    bool armed=false,active=false,blocked=false;
    bool allow() {
        if(blocked) return false;
        if(active && (consumed>=8192 || uint32_t(clock.now()-started)>=2000)) {
            blocked=true;Transport::stop();return false;
        }
        return true;
    }
    bool startWrite() {
        if(armed && !active) {active=true;started=clock.now();}
        return allow();
    }
public:
    explicit HeaderBudgetClient(Clock &c):clock(c) {}
    void armHeaders() {consumed=0;armed=true;active=false;blocked=false;}
    // Connect and TLS handshake have their own SDK timeouts. Start this
    // deadline only when HTTP writes its request, after connecting.
    void finishHeaders() {armed=false;active=false;}
    bool headerLimitHit() const {return blocked;}
    size_t write(uint8_t byte) override {return write(&byte,1);}
    size_t write(const uint8_t *data,size_t size) override {
        return startWrite()?Transport::write(data,size):0;
    }
    int available() override {return allow()?Transport::available():0;}
    uint8_t connected() override {return allow()?Transport::connected():0;}
    int read() override {
        uint8_t byte;return read(&byte,1)==1?byte:-1;
    }
    int read(uint8_t *data,size_t size) override {
        if(!allow()) {clock.idle();return -1;}
        if(active && size>8192-consumed) size=8192-consumed;
        int count=Transport::read(data,size);
        if(active && count>0) consumed+=size_t(count);
        if(active && count<=0) clock.idle();
        return count;
    }
};
}
