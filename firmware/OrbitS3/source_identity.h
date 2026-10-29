// Generated from assets/source-icons/protocols.json.
#pragma once
#include <ctype.h>
#include <string.h>
namespace s3 {
enum class SourceIcon { Airplay, Analog, Bluetooth, Cast, Dlna, Hdmi, Line, Network, Optical, Roon, Spotify, Tidal, Upnp, Unknown };
struct SourcePresentation {const char *label;SourceIcon icon;bool known;};
inline SourcePresentation identify_protocol(const char *id){
    char normalized[64]={};size_t n=0;
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(id?id:"");*p && n<63;++p){
        if(isspace(*p) || *p=='-' || *p=='_')continue;
        normalized[n++]=char(tolower(*p));
    }
    if(!strcmp(normalized,"airplay"))return {"AirPlay",SourceIcon::Airplay,true};
    if(!strcmp(normalized,"spotify") || !strcmp(normalized,"spotifyconnect"))return {"Spotify Connect",SourceIcon::Spotify,true};
    if(!strcmp(normalized,"googlecast"))return {"Google Cast",SourceIcon::Cast,true};
    if(!strcmp(normalized,"tidal") || !strcmp(normalized,"tidalconnect"))return {"Tidal Connect",SourceIcon::Tidal,true};
    if(!strcmp(normalized,"qobuz") || !strcmp(normalized,"qobuzconnect"))return {"Qobuz Connect",SourceIcon::Network,true};
    if(!strcmp(normalized,"roon"))return {"Roon",SourceIcon::Roon,true};
    if(!strcmp(normalized,"upnp"))return {"UPnP",SourceIcon::Upnp,true};
    if(!strcmp(normalized,"dlna"))return {"DLNA",SourceIcon::Dlna,true};
    if(!strcmp(normalized,"bluetooth"))return {"Bluetooth",SourceIcon::Bluetooth,true};
    return {"Network",SourceIcon::Network,false};
}
inline SourceIcon physical_source_icon(int input){
    switch(input){
        case 0:return SourceIcon::Network;case 1:return SourceIcon::Bluetooth;case 2:return SourceIcon::Line;
        case 3:return SourceIcon::Hdmi;case 4:return SourceIcon::Optical;
        default:return SourceIcon::Unknown;
    }
}
}
