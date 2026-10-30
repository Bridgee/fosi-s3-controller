// LVGL registration follows LilyGO's MIT-licensed LV_Helper.cpp.
// Copyright (c) 2024 Shenzhen Xin Yuan Electronic Technology Co., Ltd.
// Original author: Lewis He. See .deps/LilyGo-T-RGB/LICENSE.
#include "orbit_display.h"
#include <LilyGo_RGBPanel.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "screen_idle.h"
#include "orbit_ui.h"

static_assert(LV_COLOR_DEPTH==16 && LV_COLOR_16_SWAP==0,"T-RGB requires native RGB565");
namespace {
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_indev_drv_t touchDriver;
lv_disp_t *registeredDisplay=nullptr;
orbit_display::ScreenIdle screenIdle(ORBIT_SCREEN_IDLE_MS);
bool screenAwake=true,screenDimmed=false;
uint8_t savedBrightness=12;
uint8_t idleBrightness(){return savedBrightness>=8?2:1;}
void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixels) {
    static_cast<LilyGo_RGBPanel*>(driver->user_data)->pushColors(
        area->x1,area->y1,area->x2+1,area->y2+1,reinterpret_cast<uint16_t*>(pixels));
    lv_disp_flush_ready(driver);
}
void touch(lv_indev_drv_t *driver,lv_indev_data_t *data) {
    static int16_t x=0,y=0;
    bool pressed=static_cast<LilyGo_RGBPanel*>(driver->user_data)->getPoint(&x,&y,1)>0;
    data->point.x=x;data->point.y=y;
    // The entire first contact (including held/moving samples) is wake-only.
    const bool usable=screenIdle.touch(lv_tick_get(),pressed);
    orbit_ui_touch_sample(x,y,usable);
    data->state=usable?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
}
}
bool orbit_display_begin(LilyGo_RGBPanel &panel) {
    if(panel.width()!=480 || panel.height()!=480) return false;
    constexpr size_t pixels=480*40;
    auto *buffer=static_cast<lv_color_t*>(heap_caps_malloc(pixels*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!buffer) return false;
    lv_init();
    screenIdle.begin(lv_tick_get());screenAwake=true;screenDimmed=false;
    lv_disp_draw_buf_init(&drawBuffer,buffer,nullptr,pixels);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res=480;displayDriver.ver_res=480;
    displayDriver.draw_buf=&drawBuffer;displayDriver.flush_cb=flush;
    displayDriver.user_data=&panel;
    auto *display=lv_disp_drv_register(&displayDriver);
    if(!display) return false;
    registeredDisplay=display;
    // UI refresh is separate from the panel's continuous RGB pixel clock.
    lv_timer_set_period(display->refr_timer,33);
    lv_indev_drv_init(&touchDriver);touchDriver.type=LV_INDEV_TYPE_POINTER;
    touchDriver.read_cb=touch;touchDriver.user_data=&panel;
    return lv_indev_drv_register(&touchDriver)!=nullptr;
}

uint8_t orbit_display_brightness(){return savedBrightness;}
void orbit_display_set_brightness(uint8_t level) {
    savedBrightness=level<1?1:(level>16?16:level);
    if(registeredDisplay && screenAwake)
        static_cast<LilyGo_RGBPanel*>(displayDriver.user_data)->setBrightness(screenDimmed?idleBrightness():savedBrightness);
}
bool orbit_display_awake() {return screenAwake;}
bool orbit_display_dimmed() {return screenDimmed;}
void orbit_display_tick() {
    if(!registeredDisplay)return;
    const auto now=lv_tick_get();
    if(orbit_ui_alert_active())screenIdle.wake(now);
    if(orbit_ui_keep_screen_on())screenIdle.keep_awake(now);
    screenIdle.poll(now);
    // Inactive is still a wake-only contact state. The clock keeps painting,
    // so its one-second updates and visual alarm checks continue while dimmed.
    const bool dimmed=!screenIdle.awake() && orbit_ui_dim_when_idle();
    const bool awake=screenIdle.awake() || dimmed;
    if(awake==screenAwake && dimmed==screenDimmed)return;
    auto *panel=static_cast<LilyGo_RGBPanel*>(displayDriver.user_data);
    if(!awake) {
        panel->setBrightness(0);
        lv_disp_enable_invalidation(registeredDisplay,false);
        lv_timer_pause(registeredDisplay->refr_timer);
        orbit_ui_set_screen_awake(false);
    } else if(!screenAwake) {
        // Refresh latest cached state while dark, then restore the backlight.
        lv_disp_enable_invalidation(registeredDisplay,true);
        orbit_ui_set_screen_awake(true);
        lv_timer_resume(registeredDisplay->refr_timer);
        lv_obj_invalidate(lv_scr_act());lv_refr_now(registeredDisplay);
        panel->setBrightness(dimmed?idleBrightness():savedBrightness);
    } else {
        panel->setBrightness(dimmed?idleBrightness():savedBrightness);
    }
    // Never copy the temporary dim/zero level into the user's saved setting.
    screenAwake=awake;screenDimmed=dimmed;
}
