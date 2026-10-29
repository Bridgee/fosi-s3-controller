#pragma once
#include <stdint.h>
namespace s3 {
// Separate from SDK automatic reconnect: retry a stalled attempt without rapid
// disconnect loops or writes to NVS. Association waiting for DHCP gets a full minute.
struct WifiRecovery {
    uint32_t lastAttempt=0;uint8_t backoff=0;
    void connected(uint32_t now) {lastAttempt=now;backoff=0;}
    uint32_t interval(bool waitingDhcp) const {
        uint32_t delay=backoff==0?20000:backoff==1?40000:60000;
        return waitingDhcp && delay<60000?60000:delay;
    }
    uint32_t remaining(uint32_t now,bool waitingDhcp) const {
        uint32_t elapsed=now-lastAttempt,delay=interval(waitingDhcp);
        return elapsed>=delay?0:delay-elapsed;
    }
    bool due(uint32_t now,bool waitingDhcp) const {return remaining(now,waitingDhcp)==0;}
    void attempted(uint32_t now) {lastAttempt=now;if(backoff<2)++backoff;}
};
}
