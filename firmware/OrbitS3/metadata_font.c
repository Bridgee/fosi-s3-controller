#include "metadata_font.h"
#include "metadata_latin_metrics.h"

// Our generated fonts have no kerning and store plain2/4 bpp pixels in Flash.
// LVGL 8.4's generic lookup accepts range_length itself as an offset, reading
// past FORMAT0_FULL arrays at gaps/endpoints. Use strict exclusive bounds here.
// No upstream dependency is patched and no decompression buffer is needed.
// Metrics and bitmap offsets are separate naturally aligned Flash arrays;
// lookup remains direct, with no runtime unpacking or mutable glyph cache.
static uint32_t glyph_id(const lv_font_t *font,uint32_t cp) {
    if(!cp || !font || !font->dsc) return 0;
    const orbit_metadata_font_dsc_t *dsc=font->dsc;
    for(unsigned i=0;i<dsc->cmap_num;i++) {
        const lv_font_fmt_txt_cmap_t *map=&dsc->cmaps[i];
        if(cp<map->range_start || cp-map->range_start>=map->range_length) continue;
        uint32_t offset=cp-map->range_start;
        if(map->type==LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY)
            return map->glyph_id_start+offset;
        if(map->type==LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL)
            return map->glyph_id_ofs_list?map->glyph_id_start+((const uint8_t *)map->glyph_id_ofs_list)[offset]:0;
        if((map->type!=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY && map->type!=LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) || !map->unicode_list) return 0;
        unsigned low=0,high=map->list_length;
        while(low<high) {
            unsigned mid=low+(high-low)/2;
            if(map->unicode_list[mid]<offset) low=mid+1;else high=mid;
        }
        if(low==map->list_length || map->unicode_list[low]!=offset) return 0;
        if(map->type==LV_FONT_FMT_TXT_CMAP_SPARSE_TINY) return map->glyph_id_start+low;
        if(map->type==LV_FONT_FMT_TXT_CMAP_SPARSE_FULL)
            return map->glyph_id_ofs_list?map->glyph_id_start+((const uint16_t *)map->glyph_id_ofs_list)[low]:0;
        return 0;
    }
    return 0;
}
// Static assets are validated by the generator. Keep runtime bounds too so a
// bad future bitmap offset cannot expose memory beyond the Flash array.
static const orbit_metadata_metrics_t *lookup(const lv_font_t *font,uint32_t cp,uint32_t *offset) {
    uint32_t id=glyph_id(font,cp);
    if(!id) return NULL;
    const orbit_metadata_font_dsc_t *dsc=font->dsc;
    if(id>=dsc->glyph_count || !dsc->glyph_metrics || !dsc->glyph_offsets || !dsc->glyph_bitmap ||
       (dsc->bpp!=2 && dsc->bpp!=4)) return NULL;
    const orbit_metadata_metrics_t *glyph=&dsc->glyph_metrics[id];
    uint32_t off=dsc->glyph_offsets[id];
    uint32_t bytes=((uint32_t)glyph->box_w*glyph->box_h*dsc->bpp+7)/8;
    if(off>dsc->bitmap_size || bytes>dsc->bitmap_size-off) return NULL;
    *offset=off;
    return glyph;
}
bool orbit_metadata_get_glyph_dsc(const lv_font_t *font,lv_font_glyph_dsc_t *out,
                                  uint32_t cp,uint32_t next) {
    (void)next;
    if(!out) return false;
    uint32_t offset;
    const orbit_metadata_metrics_t *glyph=lookup(font,cp,&offset);
    if(!glyph) return false;
    const orbit_metadata_font_dsc_t *dsc=font->dsc;
    out->adv_w=(glyph->adv_w+8)>>4;
    out->box_w=glyph->box_w;out->box_h=glyph->box_h;
    out->ofs_x=glyph->ofs_x;out->ofs_y=glyph->ofs_y;
    out->bpp=dsc->bpp;out->is_placeholder=false;
    return true;
}
const uint8_t *orbit_metadata_get_bitmap(const lv_font_t *font,uint32_t cp) {
    uint32_t offset;
    if(!lookup(font,cp,&offset)) return NULL;
    const orbit_metadata_font_dsc_t *dsc=font->dsc;
    return dsc->glyph_bitmap+offset;
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
