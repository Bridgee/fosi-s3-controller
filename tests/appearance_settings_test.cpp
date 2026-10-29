#include "appearance_settings.h"
#include "ui_themes.h"
#include <Preferences.h>
#include <assert.h>
#include <stdio.h>
using namespace orbit_appearance;
int main() {
    auto &store=fake_preferences::state();
    begin(0);assert(theme()==0 && brightness()==12 && saved() && !store.writes);
    assert(select_theme(0) && select_brightness(12) && !store.writes);
    assert(select_theme(6) && select_brightness(7) && store.writes==2);
    begin(1);assert(theme()==6 && brightness()==7); // Restart uses saved values, not compile-time default.
    assert(select_theme(6) && select_brightness(7) && store.writes==2);
    assert(select_brightness(0) && brightness()==1);begin(0);assert(brightness()==1);
    assert(select_brightness(100) && brightness()==16);begin(0);assert(brightness()==16);
    unsigned writes=store.writes;assert(!select_theme(100) && store.writes==writes);
    store.failWrite=true;assert(!select_brightness(6) && !saved());
    begin(0);assert(brightness()==16 && saved()); // Failed commit retains old persistent setting.
    store.failWrite=false;assert(select_brightness(6) && saved());
    auto good=store.values["orbit-ui/appearance"];assert(good.size()==18);
    // Truncation, future versions, invalid brightness and missing NUL all use safe defaults.
    for(unsigned bad=0;bad<5;++bad) {
        auto record=good;
        if(bad==0)record.pop_back();if(bad==1)record[0]=9;if(bad==2)record[1]=0;
        if(bad==3)record[1]=17;if(bad==4)for(unsigned i=2;i<record.size();++i)record[i]='x';
        store.values["orbit-ui/appearance"]=record;begin(2);assert(theme()==2 && brightness()==12);
    }
    auto retired=good;memset(retired.data()+2,0,16);memcpy(retired.data()+2,"retired",7);
    store.values["orbit-ui/appearance"]=retired;begin(3);assert(theme()==3 && brightness()==6);
    store.failOpen=true;begin(0);assert(theme()==0 && brightness()==12);
    assert(!select_theme(1) && !saved());store.failOpen=false;
    assert(select_theme(1) && saved());begin(0);assert(theme()==1 && brightness()==12);
    puts("PASS: actual appearance settings store; restart persistence, stable IDs, unchanged-write suppression, brightness bounds, malformed/retired records and storage failures");
}
