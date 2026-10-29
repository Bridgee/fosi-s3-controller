#include "screen_idle.h"
#include <assert.h>
#include <stdio.h>
using orbit_display::ScreenIdle;
int main() {
    ScreenIdle idle(30000);idle.begin(100);
    idle.poll(30099);assert(idle.awake());idle.poll(30100);assert(!idle.awake());
    // First press, drag/hold and release are all swallowed; a fresh press works.
    assert(!idle.touch(31000,true) && idle.awake());
    assert(!idle.touch(61000,true));idle.poll(100000);assert(idle.awake());
    assert(!idle.touch(100001,false));idle.poll(130000);assert(idle.awake());
    assert(idle.touch(130000,true));assert(!idle.touch(130001,false));
    idle.poll(160001);assert(!idle.awake());
    ScreenIdle disabled(0);disabled.begin(0);disabled.poll(0xffffffffu);assert(disabled.awake());
    ScreenIdle shortIdle(20000);shortIdle.begin(25);shortIdle.poll(20024);assert(shortIdle.awake());
    shortIdle.poll(20025);assert(!shortIdle.awake());
    // Repeated released sensor samples and backend time passing do not renew idle.
    idle.begin(0xfffff000u);idle.touch(0xfffff100u,false);
    idle.poll(uint32_t(0xfffff000u+29999u));assert(idle.awake());
    idle.poll(uint32_t(0xfffff000u+30000u));assert(!idle.awake());
    idle.begin(0);for(uint32_t now=1000;now<=120000;now+=1000){idle.keep_awake(now);idle.poll(now);assert(idle.awake());}
    idle.poll(149999);assert(idle.awake());idle.poll(150000);assert(!idle.awake());idle.keep_awake(150001);assert(!idle.awake());
    puts("PASS:30s/20s/disabled idle, exact boundary, held touch, wake-only gesture, fresh touch and clock wrap");
}
