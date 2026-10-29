#pragma once
#include <stdint.h>

namespace s3 {
enum class LinkState : uint8_t { Starting, Searching, Ready, Limited, Waiting, Offline, Error };
enum class Node { Volume, Mute, Source, Output, Player, Position, VolumeMap, Bluetooth, VolumeLimit, Power };
enum class BluetoothStatus : uint8_t { Unknown, NotConnected, PairingMode, Searching, Connected };
enum class PowerState : uint8_t { Unknown, Online, Standby, Starting, Offline };
enum class Action { Volume, Mute, Input, Output, Toggle, Previous, Next, Standby, Wake };
inline bool power_action(Action action) {return action==Action::Standby || action==Action::Wake;}
struct Command { Action action; int value; uint32_t created; };
struct Snapshot {
    LinkState link=LinkState::Starting;
    BluetoothStatus bluetooth=BluetoothStatus::Unknown;
    PowerState power=PowerState::Unknown;
    bool powerTransition=false,powerPending=false;
    bool connected=false, volumeKnown=false, muteKnown=false;
    bool volumeLimitKnown=false;int volumeLimit=0;
    bool sourceKnown=false, outputKnown=false, playerKnown=false;
    bool playing=false, canToggle=false, canPrevious=false, canNext=false;
    bool muted=false, canPlay=false, canPause=false;
    int volume=0, volumeMax=0, volumeCeiling=-1, input=-1, output=-1;
    int elapsed=0, duration=0;
    bool positionKnown=false;
    uint64_t positionMs=0;
    uint32_t positionSampleMs=0,positionSerial=0;
    bool wifiConnected=false,automaticAddress=true;
    int wifiRssi=0;
    char localIp[16]={},deviceIp[16]={};
    uint32_t artworkRevision=0;
    char title[160]={}, artist[160]={}, album[160]={}, protocol[64]={}, artwork[512]={};
    char message[80]="Connecting to S3";
};
inline bool power_blocks_media(const Snapshot &s) {
    return s.powerPending || s.powerTransition || s.power==PowerState::Standby ||
        s.power==PowerState::Starting || s.power==PowerState::Offline;
}
inline const char *power_label(PowerState power) {
    switch(power) {
        case PowerState::Online:return "Online";
        case PowerState::Standby:return "Standby";
        case PowerState::Starting:return "Starting";
        case PowerState::Offline:return "Off";
        default:return "Unknown";
    }
}
const char *path(Node node);
}
