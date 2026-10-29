#pragma once
#include "s3_model.h"
#include <string.h>

namespace s3 {
// UI-only interpolation between timestamped device position samples. No writes,
// polling or timer allocation. The pinned LVGL tick and worker both use millis().
class PlaybackProgress {
    static constexpr uint32_t MaxSampleAge=10000;
    static constexpr uint64_t CorrectionTolerance=1200;
    uint64_t anchor=0,total=0;
    uint32_t anchorAt=0,lastSampleAt=0,seenSerial=0,seenAt=0;
    bool valid=false,running=false,seen=false,skipPending=false;
    uint32_t skipAt=0;uint64_t skipFrom=0;
    uint64_t clamp(uint64_t value) const {return value>total?total:value;}
    uint64_t estimate(uint32_t now) const {
        if(!valid)return 0;
        uint32_t delta=running?uint32_t(now-anchorAt):0;
        const uint32_t age=uint32_t(now-lastSampleAt);
        if(age>MaxSampleAge) {
            const uint32_t excess=age-MaxSampleAge;
            delta=delta>excess?delta-excess:0;
        }
        return clamp(anchor+delta);
    }
public:
    bool known() const {return valid || skipPending;}
    bool changing(uint32_t now) const {return skipPending && uint32_t(now-skipAt)<8000;}
    uint64_t milliseconds(uint32_t now) const {
        return skipPending && uint32_t(now-skipAt)<8000?0:estimate(now);
    }
    void beginTrackChange(uint32_t now) {
        skipFrom=estimate(now);skipAt=now;skipPending=true;
    }
    void observe(const Snapshot &before,const Snapshot &next,uint32_t now) {
        const uint64_t previous=estimate(now);
        const bool contextChanged=before.input!=next.input || before.duration!=next.duration ||
            strcmp(before.title,next.title) || strcmp(before.artist,next.artist) || strcmp(before.album,next.album) ||
            strcmp(before.protocol,next.protocol) || strcmp(before.deviceIp,next.deviceIp);
        const bool active=next.connected && !power_blocks_media(next) && next.sourceKnown && next.playerKnown &&
            next.input>=0 && next.input<=1 && next.duration>0;
        const bool newSample=next.positionKnown && (!seen || seenSerial!=next.positionSerial || seenAt!=next.positionSampleMs);
        if(newSample) {seen=true;seenSerial=next.positionSerial;seenAt=next.positionSampleMs;}
        const uint32_t age=uint32_t(now-next.positionSampleMs);
        if(skipPending) {
            const bool afterCommand=uint32_t(next.positionSampleMs-skipAt)<0x80000000u;
            const bool restarted=next.positionMs<=1000 || next.positionMs+CorrectionTolerance<skipFrom;
            if(!active || next.link==LinkState::Error || uint32_t(now-skipAt)>=8000 ||
               (newSample && age<=MaxSampleAge && afterCommand && restarted))skipPending=false;
        }
        const bool wasRunning=running;
        if(contextChanged || !active)valid=false;
        total=next.duration>0?uint64_t(next.duration)*1000:0;
        running=active && next.playing;
        if(!active)return;
        if(valid && running!=wasRunning) {anchor=clamp(previous);anchorAt=now;}
        if(!newSample || age>MaxSampleAge)return;
        const uint64_t target=clamp(next.positionMs+(running?age:0));
        const uint64_t predicted=estimate(now);
        const uint64_t error=target>predicted?target-predicted:predicted-target;
        // Small network jitter must not restart the local one-second phase.
        // Track/source/state transitions and actual seeks take the device value.
        if(!valid || contextChanged || running!=wasRunning || !running || error>CorrectionTolerance) {
            anchor=target;anchorAt=now;
        }
        lastSampleAt=next.positionSampleMs;valid=true;
    }
};
}
