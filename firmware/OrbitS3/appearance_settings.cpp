#include "appearance_settings.h"
#include "ui_themes.h"
#include <Preferences.h>
#include <string.h>
namespace orbit_appearance {
namespace {
struct Record {uint8_t version,level;char themeId[16];};
static_assert(sizeof(Record)==18,"Stable NVS record size");
Record current={},committed={};bool hasCommitted=false;
unsigned selected=0;
void defaults(unsigned index) {
    selected=index<orbit_theme::Count?index:0;current={};
    current.version=1;current.level=DefaultBrightness;
    strncpy(current.themeId,orbit_theme::palettes[selected].id,sizeof(current.themeId)-1);
}
bool commit() {
    if(hasCommitted && !memcmp(&current,&committed,sizeof(current)))return true;
    Preferences prefs;
    if(!prefs.begin("orbit-ui",false))return false;
    const bool ok=prefs.putBytes("appearance",&current,sizeof(current))==sizeof(current);
    prefs.end();
    if(ok){committed=current;hasCommitted=true;}
    return ok;
}
}
void begin(unsigned defaultTheme) {
    defaults(defaultTheme);committed=current;hasCommitted=true;
    Preferences prefs;if(!prefs.begin("orbit-ui",true))return;
    Record record={};
    const bool valid=prefs.getBytesLength("appearance")==sizeof(record) &&
        prefs.getBytes("appearance",&record,sizeof(record))==sizeof(record) && record.version==1 &&
        record.level>=MinBrightness && record.level<=MaxBrightness &&
        memchr(record.themeId,0,sizeof(record.themeId));
    prefs.end();
    if(!valid)return;
    // Retired/unknown palette retains brightness but uses the configured default.
    current.level=record.level;
    for(unsigned i=0;i<orbit_theme::Count;++i)if(!strcmp(record.themeId,orbit_theme::palettes[i].id)) {
        selected=i;memcpy(current.themeId,record.themeId,sizeof(current.themeId));break;
    }
    committed=current;
}
unsigned theme(){return selected;}
uint8_t brightness(){return current.level;}
bool select_theme(unsigned index) {
    if(index>=orbit_theme::Count)return false;
    selected=index;memset(current.themeId,0,sizeof(current.themeId));
    strncpy(current.themeId,orbit_theme::palettes[index].id,sizeof(current.themeId)-1);
    return commit();
}
bool select_brightness(int level) {
    current.level=level<MinBrightness?MinBrightness:(level>MaxBrightness?MaxBrightness:level);
    return commit();
}
bool saved(){return hasCommitted && !memcmp(&current,&committed,sizeof(current));}
}
