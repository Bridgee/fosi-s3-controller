#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(orbit_metadata_latin_24);
LV_FONT_DECLARE(orbit_metadata_latin_14);
LV_FONT_DECLARE(orbit_metadata_24);
LV_FONT_DECLARE(orbit_metadata_12);
// Expand the metadata line box around its existing baseline; returns added top room.
uint8_t orbit_metadata_prepare_font(lv_font_t *font,bool artist);
bool orbit_metadata_get_glyph_dsc(const lv_font_t *font,lv_font_glyph_dsc_t *out,
                                  uint32_t codepoint,uint32_t next);
const uint8_t *orbit_metadata_get_bitmap(const lv_font_t *font,uint32_t codepoint);
#ifdef __cplusplus
}
#endif
