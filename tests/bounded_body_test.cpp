#include "bounded_body.h"
#include <assert.h>
#include <string.h>
#include <string>
#include <iostream>

struct Clock {
    uint32_t ticks=0;
    uint32_t now() const {return ticks;}
    void idle() {++ticks;}
};
struct Stream {
    Clock &clock;
    std::string bytes;
    size_t offset=0;
    unsigned interval=0;
    bool keepAlive=false;
    int available() const {
        if(offset==bytes.size() || (interval && clock.now()%interval)) return 0;
        return interval?1:int(bytes.size()-offset);
    }
    int read(uint8_t *out,size_t count) {
        if(count>size_t(available())) count=available();
        memcpy(out,bytes.data()+offset,count);offset+=count;return int(count);
    }
    bool connected() const {return keepAlive || offset<bytes.size();}
};
int main() {
    char body[33];size_t count=0;
    {
        Clock c;Stream s{c,"{\"value\":38}"};
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,12,count)==s3::BodyResult::Ok);
        assert(count==12 && !memcmp(body,"{\"value\":38}",count));
    }
    {
        Clock c;Stream s{c,"{}"};
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,-1,count)==s3::BodyResult::Ok);
        assert(count==2);
    }
    {
        Clock c;Stream s{c,"x"};
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,33,count)==s3::BodyResult::TooLarge);
        assert(count==0);
    }
    {
        Clock c;Stream s{c,std::string(40,'x')};
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,-1,count)==s3::BodyResult::TooLarge);
        assert(count==32);
    }
    {
        Clock c;Stream s{c,"{}"};
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,5,count)==s3::BodyResult::Truncated);
    }
    {
        Clock c;Stream s{c,std::string(20,' ')};s.interval=4;
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,-1,count,10)==s3::BodyResult::Timeout);
        assert(c.now()==10 && count==3);
    }
    {
        Clock c;Stream s{c,"{}"};s.keepAlive=true;
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,-1,count,10)==s3::BodyResult::Timeout);
    }
    {
        Clock c;c.ticks=UINT32_MAX-5;Stream s{c,""};s.keepAlive=true;
        assert(s3::read_body(s,[&]{return s.connected();},c,body,32,-1,count,10)==s3::BodyResult::Timeout);
        assert(c.now()==4);
    }
    std::cout<<"PASS: bounded API body, unknown length, truncation, trickle deadline and clock wrap\n";
}
