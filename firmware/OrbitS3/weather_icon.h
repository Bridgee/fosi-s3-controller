#pragma once
#include <lvgl.h>
#include "weather.h"
namespace orbit_weather {
const lv_img_dsc_t *display_icon(Icon icon,bool light,bool day);
void draw_icon(lv_event_t *event,Icon icon,bool light=false,bool day=true);
}
