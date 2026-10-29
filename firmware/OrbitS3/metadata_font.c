#include "metadata_font.h"
#include "metadata_latin_metrics.h"

// Our generated fonts have no kerning and store plain2/4 bpp pixels in Flash.
// LVGL 8.4's generic lookup accepts range_length itself as an offset, reading
// past FORMAT0_FULL arrays at gaps/endpoints. Use strict exclusive bounds here.
// No upstream dependency is patched and no decompression buffer is needed.
static uint32_t glyph_id(const lv_font_t *font,uint32_t cp) {
    if(!cp) return 0;
    const lv_font_fmt_txt_dsc_t *dsc=font->dsc;
    for(unsigned i=0;i<dsc->cmap_num;i++) {
        const lv_font_fmt_txt_cmap_t *map=&dsc->cmaps[i];
        if(cp<map->range_start || cp-map->range_start>=map->range_length) continue;
        uint32_t offset=cp-map->range_start;
        if(map->type==LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY)
            return map->glyph_id_start+offset;
        if(map->type==LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL)
            return map->glyph_id_start+((const uint8_t *)map->glyph_id_ofs_list)[offset];
        unsigned low=0,high=map->list_length;
        while(low<high) {
            unsigned mid=low+(high-low)/2;
            if(map->unicode_list[mid]<offset) low=mid+1;else high=mid;
        }
        if(low==map->list_length || map->unicode_list[low]!=offset) return 0;
        if(map->type==LV_FONT_FMT_TXT_CMAP_SPARSE_TINY) return map->glyph_id_start+low;
        if(map->type==LV_FONT_FMT_TXT_CMAP_SPARSE_FULL)
            return map->glyph_id_start+((const uint16_t *)map->glyph_id_ofs_list)[low];
        return 0;
    }
    return 0;
}
bool orbit_metadata_get_glyph_dsc(const lv_font_t *font,lv_font_glyph_dsc_t *out,
                                  uint32_t cp,uint32_t next) {
    (void)next;
    uint32_t id=glyph_id(font,cp);
    if(!id) return false;
    const lv_font_fmt_txt_dsc_t *dsc=font->dsc;
    const lv_font_fmt_txt_glyph_dsc_t *glyph=&dsc->glyph_dsc[id];
    out->adv_w=(glyph->adv_w+8)>>4;
    out->box_w=glyph->box_w;out->box_h=glyph->box_h;
    out->ofs_x=glyph->ofs_x;out->ofs_y=glyph->ofs_y;
    out->bpp=dsc->bpp;out->is_placeholder=false;
    return true;
}
const uint8_t *orbit_metadata_get_bitmap(const lv_font_t *font,uint32_t cp) {
    uint32_t id=glyph_id(font,cp);
    if(!id) return NULL;
    const lv_font_fmt_txt_dsc_t *dsc=font->dsc;
    return dsc->glyph_bitmap+dsc->glyph_dsc[id].bitmap_index;
}

uint8_t orbit_metadata_prepare_font(lv_font_t *font,bool artist) {
    *font=artist?lv_font_montserrat_14:lv_font_montserrat_24;
    font->fallback=artist?&orbit_metadata_latin_14:&orbit_metadata_latin_24;
    const int ascent=artist?ORBIT_LATIN_14_ASCENT:ORBIT_LATIN_24_ASCENT;
    const int descent=artist?ORBIT_LATIN_14_DESCENT:ORBIT_LATIN_24_DESCENT;
    const int extra_top=ascent>font->line_height-font->base_line?ascent-(font->line_height-font->base_line):0;
    const int extra_bottom=descent>font->base_line?descent-font->base_line:0;
    font->line_height+=extra_top+extra_bottom;font->base_line+=extra_bottom;
    return (uint8_t)extra_top;
}
