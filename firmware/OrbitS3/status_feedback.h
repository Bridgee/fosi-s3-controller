#pragma once
#include "s3_model.h"
#include "status_identity.h"
namespace orbit_status {
// UI-only feedback from existing snapshots. No requests, retries or optimistic
// success: the short check is reserved for an actual matching online readback.
struct Feedback {
    bool pending=false,transient=false,waitSeen=false;
    s3::Action action=s3::Action::Mute;int expected=0;
    uint32_t started=0,shownAt=0,showFor=0;Icon shown=Icon::Connected;
    const char *note=nullptr;
    bool verifiable() const {return action!=s3::Action::Previous && action!=s3::Action::Next;}
    void finish(Icon icon,uint32_t now,uint32_t duration,const char *detail=nullptr) {
        pending=false;shown=icon;shownAt=now;showFor=duration;transient=duration!=0;note=detail;
    }
    void queued(s3::Action a,int value,const s3::Snapshot &before,bool accepted,uint32_t now) {
        action=a;expected=a==s3::Action::Toggle?!before.playing:value;
        pending=accepted;transient=false;waitSeen=false;started=now;note=nullptr;
        if(!accepted)finish(Icon::Error,now,2500,"Control unavailable / check S3");
    }
    bool matches(const s3::Snapshot &s) const {
        switch(action) {
            case s3::Action::Standby:return s.power==s3::PowerState::Standby && !s.powerTransition && !s.powerPending;
            case s3::Action::Wake:return s.power==s3::PowerState::Online && !s.powerTransition && !s.powerPending;
            case s3::Action::Mute:return s.muteKnown && s.muted==bool(expected);
            case s3::Action::Input:return s.sourceKnown && s.input==expected;
            case s3::Action::Output:return s.outputKnown && s.output==expected;
            case s3::Action::Volume:return s.volumeKnown && s.volume==expected;
            case s3::Action::Toggle:return s.playerKnown && s.playing==bool(expected);
            default:return false; // Track identities are not reliably exposed.
        }
    }
    void observe(const s3::Snapshot &s,uint32_t now) {
        if(s.link==s3::LinkState::Offline || (!s.connected && s.link==s3::LinkState::Error)) {
            pending=transient=false;note=nullptr;return;
        }
        if(!pending) {if(s.link==s3::LinkState::Error && shown==Icon::Confirmed)transient=false;return;}
        if(s.link==s3::LinkState::Error) {finish(Icon::Error,now,2500,"Command failed / check S3");return;}
        if(s.link==s3::LinkState::Waiting)waitSeen=true;
        if(s.connected && s.link==s3::LinkState::Ready) {
            if(verifiable() && matches(s))finish(Icon::Confirmed,now,900,"Confirmed by S3 readback");
            else if(!verifiable() && (waitSeen || uint32_t(now-started)>=1500))finish(Icon::Connected,now,0);
        }
    }
    Icon current(const s3::Snapshot &s,uint32_t now,bool live=true) {
        if(!live)return Icon::Demo;
        if(pending && uint32_t(now-started)>=8000)finish(Icon::Limited,now,2500,"Update not confirmed / check device status");
        if(transient && uint32_t(now-shownAt)>=showFor) {transient=false;note=nullptr;}
        if(transient)return shown;
        if(pending)return Icon::Waiting;
        if(s.connected && (s.powerPending || s.powerTransition))return Icon::Waiting;
        if(s.connected && s.power==s3::PowerState::Standby && !s.powerTransition)return Icon::Sleep;
        switch(s.link) {
            case s3::LinkState::Starting:case s3::LinkState::Searching:case s3::LinkState::Waiting:return Icon::Waiting;
            case s3::LinkState::Ready:return s.connected?Icon::Connected:Icon::Offline;
            case s3::LinkState::Limited:return Icon::Limited;
            case s3::LinkState::Offline:return Icon::Offline;
            default:return Icon::Error;
        }
    }
    const char *detail(const s3::Snapshot &s) const {
        return pending?(verifiable()?"Waiting for S3 readback":"Waiting for S3 status"):
            transient && note?note:s.message;
    }
};
}
