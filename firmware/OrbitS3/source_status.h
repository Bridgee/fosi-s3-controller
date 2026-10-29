#pragma once
#include "s3_model.h"
namespace s3 {
inline const char *bluetooth_status_label(BluetoothStatus status) {
    switch(status) {
        case BluetoothStatus::Connected:return "Connected";
        case BluetoothStatus::NotConnected:return "Not connected";
        case BluetoothStatus::PairingMode:return "Pairing mode";
        case BluetoothStatus::Searching:return "Searching";
        default:return "Unknown";
    }
}
// Source selection is confirmed separately from the external receiver's signal.
// No reviewed S3 interface establishes HDMI/Optical/Line signal lock.
inline const char *input_status_key(const Snapshot &s) {
    if(s.connected && s.sourceKnown) switch(s.input) {
        case 2:return "Line In signal";
        case 3:return "HDMI signal";
        case 4:return "Optical signal";
        default:break;
    }
    return "Input";
}
inline const char *input_status_value(const Snapshot &s) {
    if(s.connected && s.sourceKnown) switch(s.input) {
        case 0:return "Network";
        case 1:return "Bluetooth";
        default:break;
    }
    return "Unknown";
}
// These physical inputs are audio routes, not the network/BT player endpoint.
inline bool poll_node(Node node,const Snapshot &s) {
    return !(s.sourceKnown && s.input>=2 && s.input<=4 &&
             (node==Node::Player || node==Node::Position));
}
inline const char *connected_source_detail(const Snapshot &s) {
    if(s.sourceKnown) switch(s.input) {
        case 1:
            if(s.playerKnown && !s.title[0] && !s.artist[0] && s.duration<=0)
                return "S3 connected / Bluetooth, no track info";
            if(s.playerKnown && s.duration<=0 && !s.positionKnown)
                return "S3 connected / Bluetooth, no timeline";
            return "S3 connected / Bluetooth";
        case 2:return "Line In selected / signal unknown";
        case 3:return "HDMI selected / signal unknown";
        case 4:return "Optical selected / signal unknown";
        default:break;
    }
    return s.playerKnown && s.duration<=0 && !s.positionKnown?
        "S3 connected / audio has no timeline":"S3 connected";
}
inline const char *failed_node_detail(Node node) {
    switch(node) {
        case Node::Source:return "S3 / input status read failed";
        case Node::Volume:return "S3 / volume status read failed";
        case Node::Mute:return "S3 / mute status read failed";
        case Node::Output:return "S3 / output status read failed";
        case Node::Player:return "S3 / playback status read failed";
        case Node::Position:return "S3 / playback position read failed";
        case Node::Bluetooth:return "Bluetooth status unavailable";
        case Node::VolumeLimit:return "Volume limit unavailable";
        default:return "S3 / volume scale read failed";
    }
}
}
