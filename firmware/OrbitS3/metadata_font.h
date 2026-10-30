#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
// Two naturally aligned Flash arrays use 10 bytes/glyph rather than LVGL's
// 16-byte LARGE descriptor. Pixels and all original metrics remain unchanged.
// Only the custom callbacks below interpret this private descriptor format.
typedef struct {
    uint16_t adv_w;
    uint8_t box_w,box_h;
    int8_t ofs_x,ofs_y;
} orbit_metadata_metrics_t;
typedef char orbit_metadata_metrics_size_check[sizeof(orbit_metadata_metrics_t)==6?1:-1];
typedef struct {
    const uint8_t *glyph_bitmap;
    uint32_t bitmap_size;
    const uint32_t *glyph_offsets;
    const orbit_metadata_metrics_t *glyph_metrics;
    const lv_font_fmt_txt_cmap_t *cmaps;
    uint32_t glyph_count;
    uint16_t cmap_num;
    uint8_t bpp;
} orbit_metadata_font_dsc_t;
LV_FONT_DECLARE(orbit_numeric_48);
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
