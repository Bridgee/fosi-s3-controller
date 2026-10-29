#pragma once
#include <stdint.h>
// UI-thread only. Stable theme IDs and selected (never idle-zero) backlight level.
namespace orbit_appearance {
constexpr uint8_t MinBrightness=1,MaxBrightness=16,DefaultBrightness=12;
void begin(unsigned defaultTheme);
unsigned theme();
uint8_t brightness();
bool select_theme(unsigned index);
bool select_brightness(int level);
bool saved();
}
