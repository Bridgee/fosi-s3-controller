#pragma once
#include <lvgl.h>
extern const lv_font_t orbit_pixel_18;
extern const lv_font_t orbit_pixel_21;
extern const lv_font_t orbit_pixel_28;
extern const lv_font_t orbit_pixel_40;
extern const lv_font_t orbit_pixel_50;
extern const lv_font_t orbit_pixel_100;
namespace orbit_pixel_grid {
constexpr int Pitch=3;
inline int snap(int v){return v<0?-((-v+Pitch/2)/Pitch)*Pitch:((v+Pitch/2)/Pitch)*Pitch;}
inline int extent(int v){return ((v+2*Pitch-1)/(2*Pitch))*(2*Pitch);}
}
