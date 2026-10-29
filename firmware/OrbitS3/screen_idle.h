#pragma once
#include <stdint.h>

// Touch inactivity clock. Only an explicit local reading view can inhibit sleep;
// network/media updates never wake the display.
namespace orbit_display {
class ScreenIdle {
public:
    explicit ScreenIdle(uint32_t timeoutMs):timeout_(timeoutMs) {}
    void begin(uint32_t now) {lastTouch_=now;asleep_=held_=consume_=false;}
    bool touch(uint32_t now,bool pressed) {
        if(pressed) {
            lastTouch_=now;held_=true;
            if(asleep_) {asleep_=false;consume_=true;}
            return !consume_;
        }
        if(held_)lastTouch_=now;
        held_=false;consume_=false;
        return false;
    }
    void wake(uint32_t now){asleep_=false;lastTouch_=now;}
    void keep_awake(uint32_t now){if(!asleep_)lastTouch_=now;}
    void poll(uint32_t now) {
        if(timeout_ && !asleep_ && !held_ && uint32_t(now-lastTouch_)>=timeout_)asleep_=true;
    }
    bool awake() const {return !asleep_;}
private:
    uint32_t timeout_,lastTouch_=0;
    bool asleep_=false,held_=false,consume_=false;
};
}
