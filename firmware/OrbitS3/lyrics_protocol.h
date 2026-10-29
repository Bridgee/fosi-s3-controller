#pragma once
#include "lyrics_parser.h"
#include <ArduinoJson.h>
namespace orbit_lyrics {
inline bool json_name(JsonVariantConst value,const char *expected,bool relaxed=false){auto text=value.as<JsonString>();return !text.isNull() && text.size()<159 && valid_text(text.c_str(),text.size()) && (relaxed?same_title(text.c_str(),expected):same_name(text.c_str(),expected));}
inline bool lrclib_record(JsonVariantConst root,const Track &track,Document &document,bool relaxed=false){
    auto duration=root["duration"];if(duration.is<bool>() || !duration.is<double>() || !isfinite(duration.as<double>()) || fabs(duration.as<double>()-track.duration)>2)return false;
    if(!root["instrumental"].is<bool>()||root["instrumental"].as<bool>() || !json_name(root["trackName"],track.title,relaxed) || !json_name(root["artistName"],track.artist) ||
       (track.album[0] && !json_name(root["albumName"],track.album,relaxed)))return false;
    auto text=root["syncedLyrics"].as<JsonString>();if(text.isNull() || !parse_lrc(text.c_str(),text.size(),track,document))return false;document.provider=Provider::Lrclib;return true;
}
inline bool lrcapi_records(JsonVariantConst root,const Track &track,Document &document,bool relaxed=false){
    if(!root.is<JsonArrayConst>() || root.size()>12)return false;
    bool found=false;JsonString selected;
    for(auto entry:root.as<JsonArrayConst>()){
        if(!json_name(entry["title"],track.title,relaxed) || !json_name(entry["artist"],track.artist))continue;
        auto text=entry["lyrics"].as<JsonString>();if(text.isNull())continue;
        // The legacy service lacks trusted duration/album fields. Only an LRC
        // signature containing matching length and any known album can qualify.
        if(!parse_lrc(text.c_str(),text.size(),track,document,true))continue;
        if(found)return false; // Ambiguous candidates are never guessed.
        found=true;selected=text;
    }
    if(found && parse_lrc(selected.c_str(),selected.size(),track,document,true)){document.provider=Provider::LrcApi;return true;}return false;
}
}
