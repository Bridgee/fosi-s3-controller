#include "s3_protocol.h"
#include "source_identity.h"
#include "source_status.h"
#include <assert.h>
#include <iostream>

using namespace s3;
bool readJson(Node node,const char *json,Snapshot &state) {
    DynamicJsonDocument doc(16384);
    assert(!deserializeJson(doc,json));
    return parse(node,doc.as<JsonVariantConst>(),state);
}
int main() {
    assert(identify_protocol("AirPlay").icon==SourceIcon::Airplay);
    assert(!strcmp(identify_protocol("AirPlay").label,"AirPlay"));
    assert(identify_protocol("spotify").icon==SourceIcon::Spotify);
    assert(identify_protocol("Spotify-Connect").icon==SourceIcon::Spotify);
    assert(identify_protocol(" Google_Cast ").icon==SourceIcon::Cast);
    assert(identify_protocol("tidalconnect").icon==SourceIcon::Tidal);
    assert(!strcmp(identify_protocol("qobuzconnect").label,"Qobuz Connect"));
    assert(identify_protocol("roon").icon==SourceIcon::Roon);
    assert(identify_protocol("upnp").icon==SourceIcon::Upnp);
    assert(identify_protocol("DLNA").icon==SourceIcon::Dlna);
    assert(!identify_protocol("unknown-service").known);
    assert(identify_protocol("unknown-service").icon==SourceIcon::Network);
    assert(!identify_protocol(nullptr).known);
    assert(physical_source_icon(1)==SourceIcon::Bluetooth);
    assert(physical_source_icon(2)==SourceIcon::Line);
    assert(physical_source_icon(3)==SourceIcon::Hdmi);
    assert(physical_source_icon(4)==SourceIcon::Optical);
    assert(physical_source_icon(0)==SourceIcon::Network);
    assert(physical_source_icon(-1)==SourceIcon::Unknown);
    assert(physical_source_icon(99)==SourceIcon::Unknown);
    {Snapshot bt;
     assert(!strcmp(path(Node::Bluetooth),"bluetooth:state"));
     assert(readJson(Node::Bluetooth,R"([{"type":"bluetoothState","bluetoothState":{"connected":true,"powered":false,"pairable":false,"discoverable":false,"discovering":false}}])",bt));
     assert(bt.bluetooth==BluetoothStatus::Connected);
     assert(readJson(Node::Bluetooth,R"({"connected":false,"pairable":true})",bt) && bt.bluetooth==BluetoothStatus::PairingMode);
     assert(readJson(Node::Bluetooth,R"({"connected":false,"discoverable":true})",bt) && bt.bluetooth==BluetoothStatus::PairingMode);
     assert(readJson(Node::Bluetooth,R"({"connected":false,"discovering":true})",bt) && bt.bluetooth==BluetoothStatus::Searching);
     assert(readJson(Node::Bluetooth,R"({"connected":false,"discoverable":false,"pairable":false,"discovering":false})",bt) && bt.bluetooth==BluetoothStatus::NotConnected);
     assert(!readJson(Node::Bluetooth,R"({"connected":"true"})",bt));
     assert(!readJson(Node::Bluetooth,R"({"connected":1})",bt));
     assert(!readJson(Node::Bluetooth,R"({"connected":true,"pairable":"false"})",bt));
     assert(!readJson(Node::Bluetooth,R"({"pairable":true})",bt));
     assert(!readJson(Node::Bluetooth,R"({"error":{"name":"invalidPath"}})",bt));
     assert(!bt.canToggle && !bt.canPlay && !bt.canPause);}
    {Snapshot power;power.connected=true;StaticJsonDocument<512> p;
     assert(!encode({Action::Standby,0,0},power,p));
     assert(!encode({Action::Wake,0,0},power,p));
     assert(!strcmp(path(Node::Power),"powermanager:target"));
     assert(readJson(Node::Power,R"([{"type":"powerTarget","powerTarget":{"target":"online","nextTarget":"none","reason":"userActivity"}}])",power));
     assert(power.power==PowerState::Online && !power.powerTransition);
     assert(encode({Action::Standby,0,0},power,p));
     assert(p["path"]=="powermanager:goNetworkStandby" && p["role"]=="activate" && p["value"].is<JsonObject>() && p["value"].size()==0);
     assert(!encode({Action::Wake,0,0},power,p));
     assert(readJson(Node::Power,R"({"target":"online","nextTarget":"networkStandby"})",power));
     assert(power.power==PowerState::Online && power.powerTransition && !encode({Action::Standby,0,0},power,p));
     assert(readJson(Node::Power,R"({"target":"networkStandby","nextTarget":"none"})",power));
     assert(power.power==PowerState::Standby && !power.powerTransition);
     assert(encode({Action::Wake,0,0},power,p) && p["path"]=="powermanager:goOnline" && p["role"]=="activate");
     assert(!encode({Action::Standby,0,0},power,p));
     power.muteKnown=true;assert(!encode({Action::Mute,0,0},power,p));
     power.powerPending=true;assert(!encode({Action::Wake,0,0},power,p));power.powerPending=false;
     for(const char *bad:{"{}",R"({"target":true})",R"({"target":"paused"})",R"({"target":"networkStandby","nextTarget":3})",R"({"target":"online","nextTarget":"sleep"})"})
         assert(!readJson(Node::Power,bad,power));
     assert(readJson(Node::Power,R"({"target":"booting"})",power) && power_blocks_media(power));
     assert(readJson(Node::Power,R"({"target":"offline"})",power) && power_blocks_media(power));
     assert(!encode({Action::Wake,0,0},power,p));
     power.connected=false;power.power=PowerState::Standby;assert(!encode({Action::Wake,0,0},power,p));}
    Snapshot s;
    StaticJsonDocument<512> payload;
    assert(!encode({Action::Volume,5,0},s,payload));
    s.connected=true;
    assert(readJson(Node::Volume,R"([{"type":"i32_","i32_":38}])",s));
    assert(s.volume==38 && s.volumeKnown);
    assert(!readJson(Node::Volume,R"([{"type":"string_","string_":"99"}])",s));
    assert(s.volume==38);
    assert(!readJson(Node::Volume,R"({"error":{"name":"invalidPath"}})",s));
    assert(!encode({Action::Volume,40,0},s,payload)); // no discovered range
    assert(readJson(Node::VolumeMap,R"([{"type":"vector","vector":[-60,-40,-20,0]}])",s));
    assert(s.volumeMax==3);
    assert(!encode({Action::Volume,2,0},s,payload)); // unknown user ceiling
    assert(!s.volumeLimitKnown);
    assert(readJson(Node::VolumeLimit,R"([{"type":"i32_","i32_":2}])",s));
    assert(s.volumeLimitKnown && s.volumeLimit==2);
    assert(!readJson(Node::VolumeLimit,R"({"type":"i32_","i32_":4})",s));
    assert(!readJson(Node::VolumeLimit,R"({"type":"string_","string_":"2"})",s));
    assert(!readJson(Node::VolumeLimit,"-1",s));
    s.volumeCeiling=3;assert(!encode({Action::Volume,3,0},s,payload));
    assert(readJson(Node::VolumeLimit,"0",s));
    assert(encode({Action::Volume,0,0},s,payload));
    assert(!encode({Action::Volume,1,0},s,payload));
    assert(readJson(Node::VolumeLimit,"3",s));
    s.volumeCeiling=2;
    assert(!encode({Action::Volume,3,0},s,payload)); // scale allows it, ceiling does not
    s.volumeCeiling=0;
    assert(encode({Action::Volume,0,0},s,payload));
    assert(!encode({Action::Volume,1,0},s,payload));
    s.volumeCeiling=3;
    assert(encode({Action::Volume,2,0},s,payload));
    assert(payload["path"]=="player:volume" && payload["value"]["i32_"]==2);
    assert(payload["value"]["type"]=="i32_" && payload["role"]=="value");
    assert(!encode({Action::Volume,4,0},s,payload));
    assert(!encode({Action::Volume,-1,0},s,payload));
    assert(!readJson(Node::VolumeMap,R"([{"type":"vector","vector":[0,"bad"]}])",s));
    assert(readJson(Node::Mute,R"([{"type":"bool_","bool_":true}])",s) && s.muted);
    assert(encode({Action::Mute,0,0},s,payload));
    assert(payload["value"]["bool_"].is<bool>() && !payload["value"]["bool_"].as<bool>());
    assert(readJson(Node::Source,R"([{"type":"i32_","i32_":0}])",s));
    assert(!encode({Action::Input,0,0},s,payload)); // network is report-only
    const char *inputs[]={"","ui:/shortBluetooth","ui:/aux","ui:/hdmi","ui:/spdifin"};
    for(int i=1;i<=4;++i) {
        assert(encode({Action::Input,i,0},s,payload));
        assert(payload["path"]==inputs[i] && payload["role"]=="activate");
        assert(payload["value"].is<JsonObject>() && payload["value"].size()==0);
    }
    assert(!readJson(Node::Source,R"([{"type":"i32_","i32_":8}])",s));
    assert(readJson(Node::Output,R"([{"type":"bool_","bool_":false}])",s));
    assert(readJson(Node::Output,R"({"preferred":true})",s) && s.output==1);
    assert(readJson(Node::Output,R"({"preferred":false})",s) && s.output==0);
    assert(!readJson(Node::Output,R"({"preferred":"true"})",s));
    assert(encode({Action::Output,1,0},s,payload));
    assert(payload["path"]=="ui:/custom/audioOutputModeTrue");
    assert(readJson(Node::Player,R"([{"type":"playLogicData","playLogicData":{"state":"playing","controls":{"pause":true,"next_":true,"previous":false},"trackRoles":{"title":"Test song","icon":"http://s3/cover.jpg","mediaData":{"metaData":{"artist":"Test artist","album":"Test album","serviceID":"googlecast"}}},"status":{"duration":258000}}}])",s));
    assert(s.playing && s.canToggle && s.canNext && !s.canPrevious);
    assert(s.duration==258 && !strcmp(s.title,"Test song") && !strcmp(s.protocol,"googlecast"));
    Snapshot unicode;
    assert(!strcmp(s.album,"Test album"));
    {Snapshot release=s;assert(readJson(Node::Player,R"({"state":"playing","trackRoles":{"title":"Another recording","mediaData":{"metaData":{"artist":"Test artist"}}}})",release));
     assert(!release.album[0]&&!strcmp(release.title,"Another recording")&&!strcmp(release.artist,"Test artist"));}

    assert(readJson(Node::Player,R"({"state":"playing","trackRoles":{"title":"幸せのものさし · 幸福的尺度","mediaData":{"metaData":{"artist":"竹内まりや","serviceID":"airplay"}}}})",unicode));
    assert(!strcmp(unicode.title,"幸せのものさし · 幸福的尺度"));
    assert(!strcmp(unicode.artist,"竹内まりや"));
    assert(readJson(Node::Player,R"({"state":"paused","controls":{"pause":true,"play":true},"trackRoles":{"title":"","mediaData":{"metaData":{"title":"原文曲名"}}}})",unicode));
    assert(!strcmp(unicode.title,"原文曲名"));
    unicode.connected=true;unicode.sourceKnown=true;unicode.input=0;
    assert(encode({Action::Toggle,0,0},unicode,payload) && payload["value"]["control"]=="play");
    assert(encode({Action::Toggle,0,0},s,payload));
    assert(payload["value"]["control"]=="pause" && !payload["value"].containsKey("type"));
    assert(encode({Action::Next,0,0},s,payload));
    assert(!encode({Action::Previous,0,0},s,payload));
    assert(readJson(Node::Position,R"([{"type":"i64_","i64_":84000}])",s) && s.elapsed==84);
    assert(s.positionKnown && s.positionMs==84000 && s.positionSerial==1);
    assert(readJson(Node::Position,R"([{"type":"i64_","i64_":84567}])",s));
    assert(s.elapsed==84 && s.positionMs==84567 && s.positionSerial==2);
    {Snapshot bt;bt.sourceKnown=bt.playerKnown=true;bt.input=1;
     assert(readJson(Node::Position,R"([{"type":"i64_","i64_":-1}])",bt));
     assert(!bt.positionKnown && bt.elapsed==0 && bt.positionSerial==0);
     assert(!readJson(Node::Position,"-2",bt));
     assert(!readJson(Node::Position,"\"-1\"",bt));
     bt.duration=258;assert(!readJson(Node::Position,"-1",bt));
     bt.duration=0;bt.sourceKnown=false;assert(!readJson(Node::Position,"-1",bt));}
    assert(!readJson(Node::Position,R"([{"type":"i64_","i64_":-1000}])",s));
    assert(!readJson(Node::Position,R"([{"type":"i64_","i64_":9223372036854775807}])",s));
    assert(readJson(Node::Player,R"({"value":{"playLogicData":{"state":"paused","controls":{"play":true}}}})",s));
    assert(encode({Action::Toggle,0,0},s,payload) && payload["value"]["control"]=="play");
    assert(s.title[0]==0 && s.artwork[0]==0 && s.duration==0 && !s.positionKnown); // clear stale metadata
    assert(readJson(Node::Player,R"([{"state":"stopped","controls":{"pause":{},"next_":{}}}])",s));
    assert(!s.canToggle && !s.canNext && !encode({Action::Toggle,0,0},s,payload));
    assert(readJson(Node::Source,R"([{"type":"i32_","i32_":2}])",s));
    assert(!s.playerKnown && !s.canToggle && s.title[0]==0 && s.artwork[0]==0);
    s.connected=false;
    assert(!encode({Action::Output,0,0},s,payload));
    std::cout << "PASS: tagged values, metadata, capabilities, volume range, routing and offline writes\n";
}
