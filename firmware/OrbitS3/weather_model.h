#pragma once
#include "weather.h"
#include <stddef.h>
#include <math.h>
#include <string.h>
namespace orbit_weather {
constexpr uint32_t RefreshMs=10*60*1000,LocationMs=6*60*60*1000,MaxAgeMs=2*60*60*1000;
struct Location {char city[80]={},timezone[64]={};float latitude=0,longitude=0;};
inline bool known_code(int code) {
    switch(code){case 0:case 1:case 2:case 3:case 45:case 48:case 51:case 53:case 55:case 56:case 57:
    case 61:case 63:case 65:case 66:case 67:case 71:case 73:case 75:case 77:case 80:case 81:case 82:
    case 85:case 86:case 95:case 96:case 97:case 99:return true;default:return false;}
}
inline bool in_range(double value,double low,double high){return isfinite(value) && value>=low && value<=high;}
// A bounded, complete UTF-8 city name. Reject controls, bidi-format controls,
// embedded NUL, invalid/overlong sequences and surrogate code points.
inline bool city_name(const char *source,size_t length,char (&dest)[80]) {
    if(!source || !length || length>240)return false;
    size_t written=0;bool truncated=false;
    for(size_t i=0;i<length;) {
        const auto lead=static_cast<unsigned char>(source[i]);unsigned bytes=lead<0x80?1:(lead>=0xc2&&lead<=0xdf)?2:(lead>=0xe0&&lead<=0xef)?3:(lead>=0xf0&&lead<=0xf4)?4:0;
        if(!bytes || i+bytes>length)return false;
        uint32_t cp=bytes==1?lead:lead&((1u<<(7-bytes))-1);
        for(unsigned j=1;j<bytes;++j){auto b=static_cast<unsigned char>(source[i+j]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
        if((bytes==2&&cp<0x80)||(bytes==3&&cp<0x800)||(bytes==4&&cp<0x10000)||cp>0x10ffff ||
           (cp>=0xd800&&cp<=0xdfff)||cp<32||(cp>=0x7f&&cp<=0x9f)||(cp>=0x200b&&cp<=0x200f)||(cp>=0x2028&&cp<=0x202e)||(cp>=0x2060&&cp<=0x206f))return false;
        if(!truncated && written+bytes<sizeof(dest)){memcpy(dest+written,source+i,bytes);written+=bytes;}else truncated=true;
        i+=bytes;
    }
    dest[written]=0;return written>0 && dest[0]!=' ';
}
struct Schedule {
    uint32_t at=0,delay=0;uint8_t failures=0;
    bool due(uint32_t now)const{return uint32_t(now-at)>=delay;}
    void success(uint32_t now,uint32_t interval){at=now;delay=interval;failures=0;}
    void failure(uint32_t now,uint32_t rateLimit=0){
        static const uint32_t backoff[]={60000,300000,900000,1800000};
        at=now;delay=rateLimit?rateLimit:backoff[failures<4?failures:3];if(failures<4)++failures;
    }
};
// The age of the provider's observation, rather than the last successful GET.
// A successful repeated response cannot make an old observation look fresh.
inline bool usable(const Conditions &data,uint32_t epoch,uint32_t elapsed) {
    return elapsed<=MaxAgeMs && epoch>=data.observed && epoch-data.observed<=MaxAgeMs/1000;
}
}
