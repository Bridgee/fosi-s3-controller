#pragma once
// Retain the pinned board configuration, with explicit application overrides.
#include "../.deps/LilyGo-T-RGB/src/lv_conf.h"
#undef LV_MEMCPY_MEMSET_STD
#define LV_MEMCPY_MEMSET_STD 1
#undef LV_DISP_DEF_REFR_PERIOD
#define LV_DISP_DEF_REFR_PERIOD 33
#undef LV_FONT_FMT_TXT_LARGE
#define LV_FONT_FMT_TXT_LARGE 1

// QR generation is used only by the opt-in introduction reader.
#undef LV_USE_QRCODE
#define LV_USE_QRCODE 1

// Only numeric volume/temperature labels use 48px; use the identical UI subset.
#undef LV_FONT_MONTSERRAT_48
#define LV_FONT_MONTSERRAT_48 0
