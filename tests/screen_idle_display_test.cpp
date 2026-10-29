#include "orbit_display.h"
#include <LilyGo_RGBPanel.h>
#include <lvgl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
uint32_t fakeNow=0;bool fakePressed=false;int16_t fakeX=240,fakeY=240;
std::vector<int> calls;
lv_disp_drv_t *registeredDriver=nullptr;lv_indev_drv_t *registeredTouch=nullptr;
lv_disp_t fakeDisplay;lv_timer_t fakeRefresh;int fakeRoot=0;void *heapBuffer=nullptr;
void orbit_ui_set_screen_awake(bool awake){calls.push_back(awake?10:11);}
bool keepScreenOn=false,alertActive=false;
bool orbit_ui_alert_active(){return alertActive;}
bool orbit_ui_keep_screen_on(){return keepScreenOn;}
bool filteredPress=false;
void orbit_ui_touch_sample(int16_t,int16_t,bool pressed){filteredPress=pressed;}
static bool read(uint32_t now,bool pressed,int16_t x=240,int16_t y=240) {
    fakeNow=now;fakePressed=pressed;fakeX=x;fakeY=y;lv_indev_data_t data{};
    registeredTouch->read_cb(registeredTouch,&data);assert(data.point.x==x && data.point.y==y);
    assert(filteredPress==(data.state==LV_INDEV_STATE_PR));
    return data.state==LV_INDEV_STATE_PR;
}
static void poll(uint32_t now){fakeNow=now;orbit_display_tick();}
int main() {
    LilyGo_RGBPanel panel;assert(orbit_display_begin(panel));panel.setBrightness(12);calls.clear();
    poll(29999);assert(orbit_display_awake() && calls.empty());
    assert(read(29999,true));poll(60000);assert(orbit_display_awake());
    assert(!read(60001,false));poll(90000);assert(orbit_display_awake());
    poll(90001);assert(!orbit_display_awake() && panel.getBrightness()==0 && fakeRefresh.paused);
    assert((calls==std::vector<int>{100,24,20,11}));calls.clear();
    lv_obj_invalidate(lv_scr_act());assert(fakeRefresh.paused && calls.empty());
    poll(94000);assert(calls.empty()); // No repeated backlight or screen-off calls.
    assert(!read(95000,true));assert(!orbit_display_awake());poll(95000);
    assert(orbit_display_awake() && panel.getBrightness()==12 && !fakeRefresh.paused);
    assert((calls==std::vector<int>{25,10,21,22,23,112}));calls.clear();
    assert(!read(95100,true,400,240)); // Whole sliding/held gesture is wake-only.
    assert(!read(96000,false));assert(read(96001,true));assert(!read(96002,false));
    orbit_display_set_brightness(7);assert(orbit_display_brightness()==7);calls.clear();poll(126001);assert(orbit_display_awake());
    poll(126002);assert(!orbit_display_awake());calls.clear();
    assert(!read(126100,true));poll(126100);assert(panel.getBrightness()==7);
    assert((calls==std::vector<int>{25,10,21,22,23,107}));
    assert(!read(126200,false));calls.clear();poll(156200);assert(!orbit_display_awake() && panel.getBrightness()==0);
    orbit_display_set_brightness(4);assert(panel.getBrightness()==0 && orbit_display_brightness()==4);
    assert(!read(156300,true));poll(156300);assert(panel.getBrightness()==4);
    orbit_display_set_brightness(0);assert(panel.getBrightness()==1);
    orbit_display_set_brightness(255);assert(panel.getBrightness()==16);
    assert(!read(156400,false));calls.clear();keepScreenOn=true;
    for(unsigned now=157000;now<=277000;now+=1000){poll(now);assert(orbit_display_awake()&&!fakeRefresh.paused&&panel.getBrightness()==16);}
    assert(calls.empty());keepScreenOn=false;poll(306999);assert(orbit_display_awake());poll(307000);assert(!orbit_display_awake());
    calls.clear();alertActive=true;poll(307100);assert(orbit_display_awake()&&panel.getBrightness()==16&&!fakeRefresh.paused);
    for(unsigned now=308000;now<430000;now+=1000)poll(now);
    assert(orbit_display_awake());alertActive=false;poll(458999);assert(orbit_display_awake());poll(459000);assert(!orbit_display_awake());
    free(heapBuffer);
    puts("PASS: actual display adapter pauses refresh, preserves touch polling, consumes wake hold/drag, redraws before restoring saved brightness");
}
