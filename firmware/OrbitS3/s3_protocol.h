#pragma once
#include "s3_model.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#if ARDUINOJSON_VERSION_MAJOR != 6 || ARDUINOJSON_VERSION_MINOR < 21 || \
    (ARDUINOJSON_VERSION_MINOR == 21 && ARDUINOJSON_VERSION_REVISION < 6)
#error "Use patched ArduinoJson 6.21.6 or newer v6 (upstream numeric conversion overflow #2220)"
#endif

// Wire format is based on the two MIT community clients credited in
// PROTOCOL_REFERENCES.md. This is an independent C++ implementation.
namespace s3 {
inline const char *path(Node node) {
    switch(node) {
        case Node::Power:return "powermanager:target";
        case Node::Volume:return "player:volume";
        case Node::Mute:return "settings:/mediaPlayer/mute";
        case Node::Source:return "settings:/custom/lastAudioSource";
        case Node::Output:return "settings:/custom/audioOutputMode";
        case Node::Player:return "player:player/data/value";
        case Node::Position:return "player:player/data/playTime";
        case Node::VolumeMap:return "settings:/mediaPlayer/volumeMap";
        case Node::Bluetooth:return "bluetooth:state";
        case Node::VolumeLimit:return "settings:/mediaPlayer/maxVolume";
    }
    return "";
}
inline JsonVariantConst unwrap(JsonVariantConst value, bool first=true) {
    // A getData reply is a list of roles; volumeMap itself is also an array.
    if(first && value.is<JsonArrayConst>()) {
        if(value.size()!=1) return JsonVariantConst();
        value=value[0];
    }
    for(int depth=0;depth<6 && value.is<JsonObjectConst>();++depth) {
        const char *type=value["type"];
        if(type && !value[type].isNull()) value=value[type];
        else if(!value["value"].isNull()) value=value["value"];
        else if(!value["playLogicData"].isNull()) value=value["playLogicData"];
        else break;
    }
    return value;
}
template<size_t N> inline void copy(char (&dest)[N], const char *text) {
    snprintf(dest,N,"%s",text?text:"");
    // Keep truncated UTF-8 well formed (fonts may still need CJK support).
    size_t len=strlen(dest);
    if(text && strlen(text)>=N) {
        size_t start=len;
        while(start && (static_cast<unsigned char>(dest[start-1])&0xc0)==0x80) --start;
        if(start && static_cast<unsigned char>(dest[start-1])>=0xc0) dest[start-1]=0;
    }
}
inline bool parse(Node node, JsonVariantConst raw, Snapshot &s) {
    if(!raw["error"].isNull()) return false;
    JsonVariantConst v=unwrap(raw);
    switch(node) {
        case Node::Power: {
            if(!v.is<JsonObjectConst>() || !v["target"].is<const char*>())return false;
            const char *target=v["target"];
            PowerState power=PowerState::Unknown;
            if(!strcmp(target,"online"))power=PowerState::Online;
            else if(!strcmp(target,"networkStandby"))power=PowerState::Standby;
            else if(!strcmp(target,"booting") || !strcmp(target,"reboot"))power=PowerState::Starting;
            else if(!strcmp(target,"offline"))power=PowerState::Offline;
            else if(strcmp(target,"none"))return false;
            if(!v["nextTarget"].isNull() && !v["nextTarget"].is<const char*>())return false;
            const char *next=v["nextTarget"] | "none";
            if(strcmp(next,"none") && strcmp(next,"online") && strcmp(next,"networkStandby") &&
               strcmp(next,"booting") && strcmp(next,"reboot") && strcmp(next,"offline"))return false;
            s.power=power;s.powerTransition=strcmp(next,"none") && strcmp(next,target);
            return true;
        }
        case Node::Volume:
            if(!v.is<int>() || v.as<int>()<0 || v.as<int>()>1000) return false;
            s.volume=v.as<int>();s.volumeKnown=true;return true;
        case Node::VolumeLimit:
            if(!v.is<int>() || v.as<int>()<0 || v.as<int>()>1000 ||
               s.volumeMax<=0 || v.as<int>()>s.volumeMax) return false;
            s.volumeLimit=v.as<int>();s.volumeLimitKnown=true;return true;
        case Node::Mute:
            if(!v.is<bool>()) return false;
            s.muted=v.as<bool>();s.muteKnown=true;return true;
        case Node::Source:
            if(!v.is<int>() || v.as<int>()<0 || v.as<int>()>4) return false;
            if(s.input!=v.as<int>()) {
                s.playerKnown=false;s.playing=false;
                s.canToggle=s.canPause=s.canPlay=s.canPrevious=s.canNext=false;
                s.title[0]=s.artist[0]=s.album[0]=s.protocol[0]=s.artwork[0]=0;
                s.elapsed=s.duration=0;s.positionKnown=false;s.positionMs=0;
            }
            s.input=v.as<int>();s.sourceKnown=true;return true;
        case Node::Output:
            // Alternate structure-mode read from the optical action's
            // preferred role is used by the other reference client.
            if(v.is<JsonObjectConst>() && v["preferred"].is<bool>()) v=v["preferred"];
            if(!v.is<bool>()) return false;
            s.output=v.as<bool>()?1:0;s.outputKnown=true;return true;
        case Node::Position:
            // The inspected BT reply uses -1 for an absent timeline, not a
            // transport failure. Do not generalize other negatives/bad types.
            if(v.is<int64_t>() && v.as<int64_t>()==-1 && s.sourceKnown && s.playerKnown &&
               s.input>=0 && s.input<=1 && s.duration==0) {
                s.positionKnown=false;s.positionMs=0;s.elapsed=0;return true;
            }
            if(!v.is<int64_t>() || v.as<int64_t>()<0 || v.as<int64_t>()/1000>INT_MAX) return false;
            s.positionMs=uint64_t(v.as<int64_t>());s.elapsed=int(s.positionMs/1000);
            s.positionKnown=true;++s.positionSerial;return true;
        case Node::VolumeMap: {
            if(!v.is<JsonArrayConst>() || v.size()<2 || v.size()>1001) return false;
            for(JsonVariantConst entry:v.as<JsonArrayConst>())
                if(!entry.is<double>()) return false;
            s.volumeMax=int(v.size())-1;return true;
        }
        case Node::Bluetooth: {
            if(!v.is<JsonObjectConst>() || !v["connected"].is<bool>())return false;
            const char *flags[]={"discoverable","pairable","discovering"};
            for(const char *key:flags)
                if(!v[key].isNull() && !v[key].is<bool>())return false;
            // The observed adapter reports powered=false while connected=true.
            // Use actual link/pairability flags; never infer power-off from it.
            s.bluetooth=v["connected"].as<bool>()?BluetoothStatus::Connected:
                ((v["discoverable"] | false) || (v["pairable"] | false))?BluetoothStatus::PairingMode:
                (v["discovering"] | false)?BluetoothStatus::Searching:BluetoothStatus::NotConnected;
            return true;
        }
        case Node::Player: {
            if(!v.is<JsonObjectConst>() || !v["state"].is<const char*>()) return false;
            s.playerKnown=true;s.playing=strcmp(v["state"],"playing")==0;
            auto controls=v["controls"];
            s.canPause=controls["pause"] | false;s.canPlay=controls["play"] | false;
            s.canToggle=s.canPause || (!s.playing && s.canPlay);
            s.canPrevious=controls["previous"] | false;s.canNext=controls["next_"] | false;
            auto roles=v["trackRoles"];
            auto meta=roles["mediaData"]["metaData"];
            const char *title=roles["title"] | "";
            if(!title[0]) title=meta["title"] | "";
            const char *artist=meta["artist"] | "",*album=meta["album"] | "",*protocol=meta["serviceID"] | "";
            auto duration=v["status"]["duration"];
            int64_t ms=duration.is<int64_t>()?duration.as<int64_t>():0;
            const int seconds=ms>0 && ms/1000<=INT_MAX?int(ms/1000):0;
            if(strcmp(s.title,title) || strcmp(s.artist,artist) || strcmp(s.album,album) || strcmp(s.protocol,protocol) || s.duration!=seconds) {
                s.positionKnown=false;s.positionMs=0;s.elapsed=0;
            }
            copy(s.title,title);copy(s.artist,artist);copy(s.album,album);copy(s.protocol,protocol);
            copy(s.artwork,roles["icon"] | "");s.duration=seconds;
            return true;
        }
    }
    return false;
}
inline bool encode(const Command &c, const Snapshot &s, JsonDocument &doc) {
    doc.clear();
    if(!s.connected || s.powerPending || s.powerTransition)return false;
    if(!power_action(c.action) && power_blocks_media(s))return false;
    const char *node=nullptr;
    bool activate=false;
    switch(c.action) {
        case Action::Standby:case Action::Wake:
            if((c.action==Action::Standby && s.power!=PowerState::Online) ||
               (c.action==Action::Wake && s.power!=PowerState::Standby))return false;
            node=c.action==Action::Standby?"powermanager:goNetworkStandby":"powermanager:goOnline";
            activate=true;break;
        case Action::Volume:
            if(!s.volumeKnown || s.volumeMax<=0 || s.volumeCeiling<0 || !s.volumeLimitKnown || c.value>s.volumeLimit || c.value<0 || c.value>s.volumeMax || c.value>s.volumeCeiling) return false;
            node=path(Node::Volume);doc["value"]["type"]="i32_";doc["value"]["i32_"]=c.value;break;
        case Action::Mute:
            if(!s.muteKnown || (c.value!=0 && c.value!=1)) return false;
            node=path(Node::Mute);doc["value"]["type"]="bool_";doc["value"]["bool_"]=bool(c.value);break;
        case Action::Input: {
            if(!s.sourceKnown || c.value<1 || c.value>4) return false;
            const char *nodes[]={"","ui:/shortBluetooth","ui:/aux","ui:/hdmi","ui:/spdifin"};
            node=nodes[c.value];activate=true;break;
        }
        case Action::Output:
            if(!s.outputKnown || (c.value!=0 && c.value!=1)) return false;
            node=c.value?"ui:/custom/audioOutputModeTrue":"ui:/custom/audioOutputModeFalse";
            activate=true;break;
        case Action::Toggle:case Action::Previous:case Action::Next:
            if(!s.playerKnown || !s.sourceKnown || s.input<0 || s.input>1) return false;
            if((c.action==Action::Toggle && !s.canToggle) ||
               (c.action==Action::Previous && !s.canPrevious) ||
               (c.action==Action::Next && !s.canNext)) return false;
            node="player:player/control";activate=true;
            doc["value"]["control"]=c.action==Action::Previous?"previous":c.action==Action::Next?"next":
                (!s.playing && s.canPlay)?"play":"pause";
            break;
    }
    doc["path"]=node;doc["role"]=activate?"activate":"value";
    if(activate && doc["value"].isNull()) doc.createNestedObject("value");
    return !doc.overflowed();
}
}
