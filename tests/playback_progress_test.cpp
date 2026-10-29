#include "playback_progress.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
using namespace s3;
static Snapshot playing(uint32_t at,uint64_t ms=84000) {
    Snapshot s;s.connected=s.sourceKnown=s.playerKnown=s.positionKnown=true;
    s.playing=true;s.input=0;s.duration=258;s.positionMs=ms;
    s.positionSampleMs=at;s.positionSerial=1;
    return s;
}
int main() {
    Snapshot before,s=playing(1000);PlaybackProgress p;
    p.observe(before,s,1000);assert(p.known() && p.milliseconds(1999)==84999);
    assert(p.milliseconds(2000)==85000 && p.milliseconds(4000)==87000);
    before=s;s.positionMs=86600;s.positionSampleMs=3700;++s.positionSerial;
    p.observe(before,s,3800);assert(p.milliseconds(4000)==87000); // preserve phase with100ms jitter
    before=s;s.positionMs=89100;s.positionSampleMs=6100;++s.positionSerial;
    p.observe(before,s,6100);assert(p.milliseconds(7000)==90000);
    // Command feedback republishes the same position; it cannot reset phase/age.
    before=s;s.link=LinkState::Waiting;p.observe(before,s,7500);
    assert(p.milliseconds(8000)==91000);
    // A confirmed pause without a fresh position freezes at the current estimate.
    before=s;s.playing=false;p.observe(before,s,8000);
    assert(p.milliseconds(9500)==91000);
    before=s;s.positionMs=90800;s.positionSampleMs=9500;++s.positionSerial;
    p.observe(before,s,9500);assert(p.milliseconds(12000)==90800);
    before=s;s.playing=true;s.positionSampleMs=12000;++s.positionSerial;
    p.observe(before,s,12000);assert(p.milliseconds(13000)==91800);
    // Real seek corrections, in both directions, take effect immediately.
    before=s;s.positionMs=40000;s.positionSampleMs=14000;++s.positionSerial;
    p.observe(before,s,14000);assert(p.milliseconds(14500)==40500);
    before=s;s.positionMs=200000;s.positionSampleMs=15000;++s.positionSerial;
    p.observe(before,s,15000);assert(p.milliseconds(16000)==201000);
    // A track change with a stale position must not resurrect the previous track.
    before=s;snprintf(s.title,sizeof(s.title),"New track");s.positionKnown=false;
    p.observe(before,s,17000);assert(!p.known());
    before=s;s.positionKnown=true; // still the same token, so still stale
    p.observe(before,s,17100);assert(!p.known());
    before=s;s.positionMs=0;s.positionSampleMs=18000;++s.positionSerial;
    p.observe(before,s,18000);assert(p.known() && p.milliseconds(19000)==1000);
    // Stale snapshots cannot invent progress forever; stop after10s without a sample.
    before=s;p.observe(before,s,25000);
    assert(p.milliseconds(40000)==10000);
    // New samples unfreeze; duration bounds do not advance the actual track.
    before=s;s.positionMs=257500;s.positionSampleMs=41000;++s.positionSerial;
    p.observe(before,s,41000);assert(p.milliseconds(43000)==258000);
    before=s;s.connected=false;p.observe(before,s,44000);assert(!p.known());
    before=s;s.connected=true;p.observe(before,s,44100);assert(!p.known());
    before=s;s.positionMs=10000;s.positionSampleMs=45000;++s.positionSerial;
    p.observe(before,s,45000);assert(p.known() && p.milliseconds(45500)==10500);
    // Unknown duration, physical line input or unknown player never extrapolate.
    for(int mode=0;mode<3;++mode) {
        PlaybackProgress q;Snapshot a=playing(1000);
        if(mode==0)a.duration=0;if(mode==1)a.input=2;if(mode==2)a.playerKnown=false;
        q.observe(Snapshot{},a,1000);assert(!q.known());
    }
    // Bluetooth can supply metadata/position too, when available.
    {PlaybackProgress q;auto a=playing(1000);a.input=1;q.observe(Snapshot{},a,1000);assert(q.milliseconds(2000)==85000);}
    // Queue delivery latency and32-bit millis wrap use the same monotonic clock.
    {PlaybackProgress q;auto a=playing(UINT32_MAX-500,0);q.observe(Snapshot{},a,499);
     assert(q.milliseconds(499)==1000 && q.milliseconds(1499)==2000);}
    {PlaybackProgress q;auto a=playing(1);q.observe(Snapshot{},a,10002);assert(!q.known());}
    {PlaybackProgress q;auto a=playing(1,uint64_t(INT_MAX)*1000);a.duration=INT_MAX;
     q.observe(Snapshot{},a,1);assert(q.milliseconds(10001)==uint64_t(INT_MAX)*1000);}
    // Locally accepted skip: zero immediately; ignore cached/old-track samples.
    {PlaybackProgress q;auto a=playing(1000);q.observe(Snapshot{},a,1000);
     q.beginTrackChange(1500);assert(q.milliseconds(1500)==0 && q.milliseconds(2500)==0);
     auto old=a;a.positionMs=85000;a.positionSampleMs=2200;++a.positionSerial;
     q.observe(old,a,2200);assert(q.milliseconds(2800)==0);
     old=a;snprintf(a.title,sizeof(a.title),"Next track");a.positionMs=85800;
     a.positionSampleMs=2800;++a.positionSerial;
     q.observe(old,a,2800);assert(q.known() && q.milliseconds(2800)==0);
     old=a;a.positionKnown=false;
     q.observe(old,a,2900);assert(q.known() && q.milliseconds(2900)==0);
     old=a;a.positionKnown=true;a.positionMs=400;a.positionSampleMs=3000;++a.positionSerial;
     q.observe(old,a,3000);assert(q.milliseconds(4000)==1400);}
    // Repeated metadata (restart previous track), errors and bounded timeout.
    {PlaybackProgress q;auto a=playing(1000);q.observe(Snapshot{},a,1000);q.beginTrackChange(1500);
     auto old=a;a.positionMs=0;a.positionSampleMs=2500;++a.positionSerial;
     q.observe(old,a,2500);assert(q.milliseconds(3500)==1000);}
    {PlaybackProgress q;auto a=playing(1000);q.observe(Snapshot{},a,1000);q.beginTrackChange(1500);
     auto old=a;a.link=LinkState::Error;q.observe(old,a,2000);assert(q.milliseconds(2000)==85000);}
    {PlaybackProgress q;auto a=playing(1000);q.observe(Snapshot{},a,1000);q.beginTrackChange(1500);
     assert(q.milliseconds(9499)==0 && q.milliseconds(9500)>84000);}
    puts("PASS: local progress cadence/jitter, pause/resume, seeks, stale/context/offline guards, duration and millis wrap");
}
