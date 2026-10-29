#include "status_feedback.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
using namespace orbit_status;
static s3::Snapshot ready() {s3::Snapshot s;s.link=s3::LinkState::Ready;s.connected=true;s.muteKnown=s.sourceKnown=s.outputKnown=s.volumeKnown=s.playerKnown=true;s.input=s.output=0;s.volume=38;s.playing=true;return s;}
int main() {
    Feedback f;auto s=ready();assert(f.current(s,0)==Icon::Connected);
    s.playing=false;assert(f.current(s,0)==Icon::Connected); // Paused is never Sleep.
    f.queued(s3::Action::Mute,1,s,true,100);f.observe(s,200);
    assert(f.current(s,2200)==Icon::Waiting); // Ready response alone is not confirmation.
    s.muted=true;f.observe(s,2300);assert(f.current(s,2300)==Icon::Confirmed);
    assert(f.current(s,3199)==Icon::Confirmed);assert(f.current(s,3200)==Icon::Connected);
    for(auto a:{s3::Action::Input,s3::Action::Output,s3::Action::Volume}) {
        f.queued(a,2,s,true,4000);f.observe(s,4100);assert(f.current(s,4100)==Icon::Waiting);
        s.input=s.output=s.volume=2;f.observe(s,4200);assert(f.current(s,4200)==Icon::Confirmed);
        s=ready();
    }
    f.queued(s3::Action::Toggle,0,s,true,5000);f.observe(s,5100);assert(f.current(s,5100)==Icon::Waiting);
    s.playing=false;f.observe(s,5200);assert(f.current(s,5200)==Icon::Confirmed);
    f.queued(s3::Action::Mute,1,s,true,6000);s.link=s3::LinkState::Limited;s.muted=true;f.observe(s,6100);
    assert(f.current(s,6100)==Icon::Waiting);assert(f.current(s,14000)==Icon::Limited);
    s=ready();f.queued(s3::Action::Next,0,s,true,15000);f.observe(s,15100);assert(f.current(s,15100)==Icon::Waiting);
    s.link=s3::LinkState::Waiting;f.observe(s,15200);s.link=s3::LinkState::Ready;f.observe(s,15300);assert(f.current(s,15300)==Icon::Connected);
    f.queued(s3::Action::Mute,1,s,false,16000);assert(f.current(s,16000)==Icon::Error);assert(f.current(s,18500)==Icon::Connected);
    f.queued(s3::Action::Mute,1,s,true,19000);s.link=s3::LinkState::Offline;s.connected=false;f.observe(s,19100);assert(f.current(s,19100)==Icon::Offline);
    s=ready();assert(f.current(s,19200)==Icon::Connected); // No success replay after reconnect.
    f.queued(s3::Action::Mute,1,s,true,0xffffff00u);assert(f.current(s,0xffffff00u+7999u)==Icon::Waiting);
    assert(f.current(s,0xffffff00u+8000u)==Icon::Limited);
    assert(f.current(s,100,false)==Icon::Demo);
    f=Feedback{};s=ready();s.power=s3::PowerState::Online;
    f.queued(s3::Action::Standby,0,s,true,100);f.observe(s,200);assert(f.current(s,200)==Icon::Waiting);
    s.power=s3::PowerState::Standby;s.powerTransition=true;f.observe(s,300);assert(f.current(s,300)==Icon::Waiting);
    s.powerTransition=false;s.powerPending=true;f.observe(s,400);assert(f.current(s,400)==Icon::Waiting);
    s.powerPending=false;f.observe(s,500);assert(f.current(s,500)==Icon::Confirmed);
    assert(f.current(s,1400)==Icon::Sleep);
    f.queued(s3::Action::Wake,0,s,true,1500);f.observe(s,1600);assert(f.current(s,1600)==Icon::Waiting);
    s.power=s3::PowerState::Online;f.observe(s,1700);assert(f.current(s,1700)==Icon::Confirmed);
    assert(f.current(s,2600)==Icon::Connected);
    s.power=s3::PowerState::Standby;s.connected=false;s.link=s3::LinkState::Offline;
    assert(f.current(s,2700)==Icon::Offline);
    s=ready();s.power=s3::PowerState::Unknown;s.playing=false;assert(f.current(s,2800)==Icon::Connected);
    puts("PASS: matching readback confirmation, no ready-only/paused/track false success, timeout, disconnect, reject and clock wrap");
}
