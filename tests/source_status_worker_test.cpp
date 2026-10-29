#include <initializer_list>
#include "s3_protocol.h"
#include "source_status.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <atomic>
#define ORBIT_ARTWORK_ENABLED 0
using namespace s3;
// The real TLS lease policy has dedicated async tests; this fixture models admission.
namespace orbit_intro_async {
struct MediaLease {explicit MediaLease(bool){} explicit operator bool()const{return true;}};
inline void preemptAi(){}
}
namespace s3 {
enum class Result {Ok,Missing,Error,Transport};
struct ArtworkRequest {char url[512]={},origin[192]={};uint32_t revision=0;};
static Snapshot state,published,displayed;
static DynamicJsonDocument doc(4096);
static std::atomic<bool> bluetoothInfoRequested(false),powerInFlight(false),online(false);
static PowerState expectedPower=PowerState::Unknown;static uint32_t powerCommandAt=0;
static const char *powerTarget="online",*nextPower="none";static bool queueSendAccept=true;
static bool curveSettled=true,outputPreferred=false,automatic=false,interruptPoll=false,interruptVolumeOnly=false;
static uint32_t clockNow=1000,lastVolume=0;
static bool queuedControl=false,queuedVolume=false;static Command queued{Action::Volume,40,1000};
static unsigned posts=0;static Result postResult=Result::Ok;
using String=std::string;
constexpr int pdTRUE=1;
static uint32_t nextPoll=0,nextDiscovery=0;
static uint8_t pollFailures=0;static unsigned targetClears=0;
struct Origin : std::string {
    using std::string::operator=;
    bool startsWith(const char *prefix)const{return compare(0,strlen(prefix),prefix)==0;}
};
static Origin apiBase;
inline void vTaskDelay(unsigned){}
#define pdMS_TO_TICKS(value) (value)
static int commands=1,volumes=2,artworkRequests=3,input=1,failNode=-1;
static Result failureResult=Result::Error;
static int64_t position=-1;static int duration=0;
static unsigned dropped=0,publishes=0;
static std::vector<Node> reads;
uint32_t millis(){return clockNow;}
int uxQueueMessagesWaiting(int queue){return interruptPoll || (queue==volumes && interruptVolumeOnly);}
int xQueueReceive(int queue,Command *c,int) {
    bool &present=queue==commands?queuedControl:queuedVolume;
    if(!present)return 0;
    present=false;*c=queued;return pdTRUE;
}
Result request(const char*,JsonDocument&,const String*){++posts;return postResult;}
void xQueueOverwrite(int,const ArtworkRequest*){}
int xQueueOverwrite(int,const Command *c){queued=*c;queuedVolume=true;return pdTRUE;}
int xQueueSend(int,const Command *c,int){if(!queueSendAccept)return 0;queued=*c;queuedControl=true;return pdTRUE;}
void xQueueReset(int queue){if(queue==commands)queuedControl=false;else if(queue==volumes)queuedVolume=false;}
void dropCommands(){++dropped;queuedControl=queuedVolume=false;powerInFlight.store(false);}
void clearTarget(Snapshot &s){s=Snapshot{};apiBase.clear();++targetClears;}
void publish(Snapshot &s){published=displayed=s;online.store(s.connected);++publishes;}
Result read(Node n,Snapshot &s,JsonDocument &json,bool&) {
    reads.push_back(n);if(int(n)==failNode)return failureResult;
    json.clear();
    switch(n) {
        case Node::Power:json["target"]=powerTarget;json["nextTarget"]=nextPower;break;
        case Node::Source:json.set(input);break;
        case Node::Volume:json.set(45);break;
        case Node::Mute:json.set(false);break;
        case Node::Output:json.set(false);break;
        case Node::Position:json.set(position);break;
        case Node::VolumeMap:s.volumeMax=100;return Result::Ok;
        case Node::VolumeLimit:json.set(52);break;
        case Node::Player:
            json["state"]="playing";json["status"]["duration"]=duration;break;
        case Node::Bluetooth:
            json["connected"]=true;json["pairable"]=false;json["discoverable"]=false;json["discovering"]=false;break;
    }
    return parse(n,json.as<JsonVariantConst>(),s)?Result::Ok:Result::Error;
}
// ACTUAL_SEND
void command_cycle() {
    uint32_t now=millis();
    do {
// ACTUAL_COMMAND
    } while(false);
}
void cycle() {
// ACTUAL_POLL
}
void reset() {
    state=Snapshot{};state.volumeMax=100;published=displayed=Snapshot{};
    automatic=false;apiBase.clear();pollFailures=0;targetClears=0;
    expectedPower=PowerState::Unknown;powerCommandAt=0;powerInFlight.store(false);online.store(false);
    powerTarget="online";nextPower="none";queueSendAccept=true;curveSettled=true;outputPreferred=false;
    bluetoothInfoRequested.store(false);interruptPoll=false;nextPoll=nextDiscovery=0;failNode=-1;input=1;position=-1;
    duration=0;dropped=publishes=posts=0;clockNow=1000;lastVolume=0;queuedControl=queuedVolume=interruptVolumeOnly=false;postResult=Result::Ok;reads.clear();failureResult=Result::Error;
}
}
int main() {
    using namespace s3;
    reset();cycle();assert(publishes==1 && published.connected && published.link==LinkState::Ready);
    assert(published.playing && !published.positionKnown && published.duration==0);
    assert(!strcmp(published.message,"S3 connected / Bluetooth, no track info"));
    assert(!published.canToggle && !published.canNext && published.volumeCeiling==-1);
    assert(reads.size()==9 && published.bluetooth==BluetoothStatus::Connected);
    for(auto n:{Node::Source,Node::Volume,Node::Mute,Node::Output,Node::Player,Node::Position}) {
        reset();failNode=int(n);cycle();assert(published.connected && published.link==LinkState::Limited);
        assert(!strcmp(published.message,failed_node_detail(n)));
    }
    for(auto result:{Result::Missing,Result::Transport}) {
        reset();failNode=int(Node::Volume);failureResult=result;cycle();
        assert(published.link==(result==Result::Transport?LinkState::Offline:LinkState::Limited));
        assert(published.connected==(result!=Result::Transport));
        if(result==Result::Transport)assert(dropped==1);
    }
    // Unknown sources and malformed/contradictory timelines stay warnings.
    reset();position=-1000;cycle();assert(published.link==LinkState::Limited);
    reset();duration=258000;cycle();assert(published.link==LinkState::Limited);
    reset();input=0;position=-1;cycle();assert(published.link==LinkState::Ready && !published.positionKnown);
    for(int source=2;source<=4;++source) {
        reset();input=source;cycle();assert(published.connected && published.link==LinkState::Ready && reads.size()==6);
        assert(!published.playerKnown && !published.playing && !published.positionKnown);
        assert(!strcmp(published.message,connected_source_detail(published)));
        assert(strstr(published.message,"selected / signal unknown"));
        assert(!strcmp(input_status_value(published),"Unknown"));
        assert(!strcmp(input_status_key(published),source==2?"Line In signal":source==3?"HDMI signal":"Optical signal"));
        assert(!published.canPlay && !published.canPause && !published.canToggle && !published.canNext && !published.canPrevious);
        for(Node n:reads)assert(n!=Node::Player && n!=Node::Position);
        reset();input=source;failNode=int(Node::Output);cycle();assert(published.link==LinkState::Limited);
    }
    // The selected-input node is not evidence of signal presence/absence.
    // Switching from network metadata must clear all stale track/control state.
    for(int source=2;source<=4;++source) {
        reset();input=source;state.input=0;state.sourceKnown=state.playerKnown=true;
        state.playing=state.canPlay=state.canPause=state.canToggle=state.canNext=state.canPrevious=true;
        state.positionKnown=true;state.positionMs=90000;state.duration=240;
        copy(state.title,"Old network track");copy(state.artist,"Old artist");copy(state.protocol,"AirPlay");copy(state.artwork,"http://192.0.2.2/cover");
        cycle();assert(published.link==LinkState::Ready && published.artworkRevision==1);
        assert(!published.title[0] && !published.artist[0] && !published.protocol[0] && !published.artwork[0]);
        assert(!published.canPlay && !published.canPause && !published.positionKnown && published.duration==0);
        reset();input=source;failNode=int(Node::Source);cycle();
        assert(published.link==LinkState::Limited && !published.sourceKnown);
        assert(!strcmp(input_status_key(published),"Input") && !strcmp(input_status_value(published),"Unknown"));
        reset();input=source;failNode=int(Node::Volume);failureResult=Result::Transport;cycle();
        assert(!published.connected && published.link==LinkState::Offline);
        assert(!strcmp(input_status_key(published),"Input") && !strcmp(input_status_value(published),"Unknown"));
    }
    // Optional BT telemetry failure must not invent an S3 network failure.
    for(auto result:{Result::Error,Result::Missing,Result::Transport}) {
        reset();failNode=int(Node::Bluetooth);failureResult=result;cycle();
        assert(published.connected && published.link==LinkState::Ready && published.bluetooth==BluetoothStatus::Unknown);
    }
    reset();input=0;position=0;duration=258000;cycle();assert(reads.size()==8 && published.bluetooth==BluetoothStatus::Unknown);
    reset();input=0;position=0;duration=258000;bluetoothInfoRequested.store(true);cycle();
    assert(reads.size()==9 && published.bluetooth==BluetoothStatus::Connected);
    reset();input=3;bluetoothInfoRequested.store(true);cycle();
    assert(reads.size()==7 && published.bluetooth==BluetoothStatus::Connected);
    reset();interruptPoll=true;cycle();assert(published.link==LinkState::Waiting);
    assert(!strcmp(published.message,"Reading S3 status"));
    // Never skip player reads on an unverified/failed source node.
    reset();state.input=3;state.sourceKnown=true;failNode=int(Node::Source);cycle();
    assert(reads.size()==8 && published.link==LinkState::Limited && !published.sourceKnown);
    // Optional maximum failure disables volume writes, not a healthy S3 link.
    for(auto result:{Result::Error,Result::Missing,Result::Transport}) {
        reset();failNode=int(Node::VolumeLimit);failureResult=result;cycle();
        assert(published.connected && published.link==LinkState::Ready && !published.volumeLimitKnown);
    }
    reset();interruptVolumeOnly=true;cycle();
    assert(published.link==LinkState::Waiting && published.sourceKnown && published.volumeKnown && published.volumeLimitKnown);
    assert(reads.size()==4 && reads[0]==Node::Power && reads[1]==Node::Source && reads[2]==Node::VolumeLimit && reads[3]==Node::Volume);
    // An uncertain write stops controls and checks the same target first.
    reset();cycle();state.volumeCeiling=100;queuedVolume=true;postResult=Result::Transport;
    command_cycle();assert(posts==1 && dropped==1 && state.link==LinkState::Waiting && !state.connected);
    assert(state.sourceKnown && state.input==1 && !state.volumeKnown && !state.volumeLimitKnown);
    assert(!nextDiscovery);command_cycle();assert(posts==1); // No replay.
    clockNow=nextPoll;cycle();assert(published.connected && published.link==LinkState::Ready && published.input==1);
    // Continuous volume writes cannot move an existing readback deadline later.
    reset();cycle();state.volumeCeiling=100;nextPoll=1500;queuedVolume=true;command_cycle();
    assert(posts==1 && nextPoll==1500);
    clockNow=1200;queued.created=1200;queuedVolume=true;command_cycle();assert(posts==1 && queuedVolume);
    clockNow=1300;queued.created=1300;command_cycle();assert(posts==2 && nextPoll==1500);
    // Real standby reads one endpoint, clears stale media and remains reachable.
    reset();cycle();reads.clear();powerTarget="networkStandby";state.volumeKnown=state.muteKnown=true;
    copy(state.artwork,"http://192.0.2.2/old-cover");clockNow=nextPoll;cycle();
    assert(published.connected && published.power==PowerState::Standby && published.link==LinkState::Ready);
    assert(reads.size()==1 && reads[0]==Node::Power);
    assert(!published.sourceKnown && !published.volumeKnown && !published.playerKnown && !published.playing && !published.artwork[0]);
    assert(published.artworkRevision==1 && !published.canToggle && !published.volumeLimitKnown);
    // Wake is an explicit online action, with no input-switch/transport side effect.
    assert(send(Action::Wake,0) && powerInFlight.load());
    assert(!send(Action::Wake,0) && !send(Action::Mute,0));
    command_cycle();assert(posts==1 && state.powerPending && expectedPower==PowerState::Online);
    clockNow=nextPoll;cycle();assert(published.power==PowerState::Standby && published.powerPending && published.link==LinkState::Waiting);
    assert(posts==1);powerTarget="online";clockNow=nextPoll;cycle();
    assert(published.connected && published.power==PowerState::Online && !published.powerPending && !powerInFlight.load());
    assert(published.sourceKnown && published.volumeKnown && posts==1);
    // A power command discards unsent media; failures and timeouts do not replay.
    reset();cycle();queuedControl=queuedVolume=true;
    assert(send(Action::Standby,0) && queuedControl && !queuedVolume && queued.action==Action::Standby);
    command_cycle();assert(posts==1 && state.powerPending);
    clockNow=nextPoll;cycle();assert(published.power==PowerState::Online && published.powerPending);
    clockNow=powerCommandAt+8000;cycle();assert(!published.powerPending && !powerInFlight.load() && published.link==LinkState::Error);
    command_cycle();assert(posts==1);
    reset();cycle();queueSendAccept=false;assert(!send(Action::Standby,0) && !powerInFlight.load());
    reset();cycle();assert(send(Action::Standby,0));postResult=Result::Error;command_cycle();
    assert(posts==1 && state.link==LinkState::Error && !state.powerPending && !powerInFlight.load());
    reset();cycle();assert(send(Action::Standby,0));postResult=Result::Transport;command_cycle();
    assert(posts==1 && !state.connected && state.powerPending && powerInFlight.load());
    powerTarget="networkStandby";clockNow=nextPoll;cycle();assert(published.power==PowerState::Standby && !published.powerPending && posts==1);
    reset();failNode=int(Node::Power);cycle();assert(published.connected && published.power==PowerState::Unknown && !send(Action::Standby,0));
    // One automatic-target read failure gets one bounded read-only retry, not mDNS.
    reset();automatic=true;apiBase="http://192.0.2.23";input=0;position=0;duration=240;cycle();
    strcpy(state.title,"Remembered track");strcpy(state.protocol,"AirPlay");
    strcpy(state.artwork,"http://192.0.2.23/cover");const auto retainedRevision=state.artworkRevision;
    clockNow=nextPoll;failNode=int(Node::Power);failureResult=Result::Transport;cycle();
    assert(!published.connected && published.link==LinkState::Waiting && targetClears==0);
    assert(apiBase=="http://192.0.2.23" && nextDiscovery==0 && nextPoll==clockNow+1000);
    assert(!strcmp(published.title,"Remembered track") && !strcmp(published.protocol,"AirPlay"));
    assert(!strcmp(published.artwork,"http://192.0.2.23/cover") && published.artworkRevision==retainedRevision);
    assert(!published.sourceKnown && !published.playerKnown && !published.volumeKnown && !published.muteKnown);
    assert(!published.outputKnown && !published.volumeLimitKnown && !published.canPlay && !published.canPause);
    assert(!send(Action::Mute,1) && !send(Action::Volume,50) && !send(Action::Input,3) && posts==0);
    clockNow=nextPoll;failNode=-1;cycle();
    assert(published.connected && published.link==LinkState::Ready && pollFailures==0 && targetClears==0);
    assert(apiBase=="http://192.0.2.23" && posts==0); // No control replay.
    // A later isolated failure gets a fresh retry; consecutive failure clears the target.
    failNode=int(Node::Power);failureResult=Result::Transport;clockNow=nextPoll;cycle();
    assert(published.link==LinkState::Waiting && targetClears==0);
    clockNow=nextPoll;cycle();assert(!published.connected && published.link==LinkState::Offline);
    assert(targetClears==1 && apiBase.empty() && nextDiscovery==clockNow+5000 && posts==0);
    // A real standby response on the retry also recovers without media writes.
    reset();automatic=true;apiBase="http://192.0.2.23";cycle();
    failNode=int(Node::Power);failureResult=Result::Transport;clockNow=nextPoll;cycle();assert(pollFailures==1);
    failNode=-1;powerTarget="networkStandby";clockNow=nextPoll;cycle();
    assert(published.connected && published.power==PowerState::Standby && pollFailures==0 && posts==0);
    // A configured manual address retains the existing offline/backoff policy.
    reset();apiBase="http://192.0.2.23";failNode=int(Node::Power);failureResult=Result::Transport;cycle();
    assert(published.link==LinkState::Offline && targetClears==0 && nextPoll==clockNow+5000);
    puts("PASS: dynamic volume maximum, bounded command recovery/readback and source context; actual poll loop accepts no-timeline Bluetooth, keeps real failures, skips non-player physical nodes and distinguishes interrupted polling");
}
