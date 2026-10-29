#pragma once
#include <stddef.h>
#include <stdint.h>

namespace s3 {
constexpr size_t ApiBodyLimit=32*1024;
constexpr uint32_t ApiBodyDeadlineMs=2500;
enum class BodyResult { Ok, Timeout, Truncated, TooLarge };

// Stream reads must be nonblocking. An overall deadline also bounds trickling
// and unknown-length responses; a per-byte stream timeout cannot do that.
template<class Stream,class Connected,class Clock>
BodyResult read_body(Stream &stream,Connected connected,Clock &clock,
                     char *buffer,size_t limit,int expected,size_t &received,
                     uint32_t deadlineMs=ApiBodyDeadlineMs) {
    received=0;
    if(expected>=0 && size_t(expected)>limit) return BodyResult::TooLarge;
    const uint32_t started=clock.now();
    for(;;) {
        if(expected>=0 && received==size_t(expected)) return BodyResult::Ok;
        if(uint32_t(clock.now()-started)>=deadlineMs) return BodyResult::Timeout;
        int available=stream.available();
        if(available>0) {
            if(received==limit) return BodyResult::TooLarge;
            size_t amount=size_t(available);
            if(amount>1024) amount=1024;
            if(amount>limit-received) amount=limit-received;
            if(expected>=0 && amount>size_t(expected)-received) amount=size_t(expected)-received;
            int count=stream.read(reinterpret_cast<uint8_t*>(buffer)+received,amount);
            if(count>0) received+=size_t(count);
        } else if(!connected()) {
            return expected<0?BodyResult::Ok:BodyResult::Truncated;
        }
        clock.idle();
    }
}
}
