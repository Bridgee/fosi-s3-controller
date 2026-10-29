#pragma once
#include <stdint.h>
class LilyGo_RGBPanel;
// Official panel/touch APIs, with a small partial LVGL draw buffer.
bool orbit_display_begin(LilyGo_RGBPanel &panel);

// Call on the LVGL/UI thread after each lv_timer_handler(). Never sleeps the MCU/S3.
void orbit_display_tick();
bool orbit_display_awake();

// Selected awake brightness, 1..16. Sleep-only zero is never a user setting.
void orbit_display_set_brightness(uint8_t level);
uint8_t orbit_display_brightness();
