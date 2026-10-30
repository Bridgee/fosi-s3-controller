#include "metadata_font.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
static uint64_t mix(uint64_t h,uint32_t value) {
    for(unsigned i=0;i<4;++i) {h^=(value>>(8*i))&255;h*=UINT64_C(1099511628211);}
    return h;
}
static void bounds_test(void) {
    const uint8_t pixels[]={0xc0,0xc0};
    uint32_t offsets[]={0,0,1};
    orbit_metadata_metrics_t metrics[]={{0},{16,1,1,0,0},{32,1,1,0,0}};
    lv_font_fmt_txt_cmap_t map={.range_start='A',.range_length=2,.glyph_id_start=1,
        .unicode_list=NULL,.glyph_id_ofs_list=NULL,.list_length=0,.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY};
    orbit_metadata_font_dsc_t data={.glyph_bitmap=pixels,.bitmap_size=sizeof pixels,
        .glyph_offsets=offsets,.glyph_metrics=metrics,.cmaps=&map,.glyph_count=3,.cmap_num=1,.bpp=2};
    lv_font_t font={.get_glyph_dsc=orbit_metadata_get_glyph_dsc,.get_glyph_bitmap=orbit_metadata_get_bitmap,.dsc=&data};
    lv_font_glyph_dsc_t out={0};
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'A',0) && out.adv_w==1);
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'B',0) && out.adv_w==2);
    assert(!orbit_metadata_get_glyph_dsc(&font,&out,'C',0));
    assert(!orbit_metadata_get_bitmap(&font,'C'));
    const uint8_t dense_ids[]={1,0};map.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL;map.glyph_id_ofs_list=dense_ids;map.list_length=2;
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'A',0) && out.adv_w==2);
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'B',0) && out.adv_w==1);
    assert(!orbit_metadata_get_glyph_dsc(&font,&out,'C',0));
    map.glyph_id_ofs_list=NULL;assert(!orbit_metadata_get_bitmap(&font,'A'));
    const uint16_t sparse[]={0,2},sparse_ids[]={1,0};
    map.range_length=3;map.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY;map.unicode_list=sparse;
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'A',0) && out.adv_w==1);
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'C',0) && out.adv_w==2);
    assert(!orbit_metadata_get_bitmap(&font,'B') && !orbit_metadata_get_bitmap(&font,'D'));
    map.type=LV_FONT_FMT_TXT_CMAP_SPARSE_FULL;map.glyph_id_ofs_list=sparse_ids;
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'A',0) && out.adv_w==2);
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'C',0) && out.adv_w==1);
    map.glyph_id_ofs_list=NULL;assert(!orbit_metadata_get_bitmap(&font,'A'));
    map.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY;map.unicode_list=NULL;assert(!orbit_metadata_get_bitmap(&font,'A'));
    map.type=(lv_font_fmt_txt_cmap_type_t)255;assert(!orbit_metadata_get_bitmap(&font,'A'));
    map.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY;map.range_length=2;map.list_length=0;
    map.glyph_id_start=3;assert(!orbit_metadata_get_bitmap(&font,'A'));assert(!orbit_metadata_get_glyph_dsc(&font,&out,'A',0));
    map.glyph_id_start=1;offsets[1]=UINT32_MAX;assert(!orbit_metadata_get_bitmap(&font,'A'));assert(!orbit_metadata_get_glyph_dsc(&font,&out,'A',0));
    offsets[1]=sizeof pixels;assert(!orbit_metadata_get_bitmap(&font,'A'));
    offsets[1]=1;metrics[1].box_w=8;assert(!orbit_metadata_get_bitmap(&font,'A'));
    offsets[1]=0;metrics[1].box_w=255;metrics[1].box_h=255;assert(!orbit_metadata_get_bitmap(&font,'A'));
    metrics[1].box_w=1;metrics[1].box_h=1;data.bitmap_size=0;assert(!orbit_metadata_get_bitmap(&font,'A'));
    data.bitmap_size=sizeof pixels;data.bpp=255;assert(!orbit_metadata_get_bitmap(&font,'A'));
    data.bpp=2;metrics[1].box_w=0;metrics[1].box_h=0;offsets[1]=sizeof pixels;
    assert(orbit_metadata_get_glyph_dsc(&font,&out,'A',0) && orbit_metadata_get_bitmap(&font,'A')==pixels+sizeof pixels);
    offsets[1]=sizeof pixels+1;assert(!orbit_metadata_get_bitmap(&font,'A'));
    assert(!orbit_metadata_get_bitmap(&font,UINT32_MAX));
    assert(!orbit_metadata_get_bitmap(NULL,'A'));assert(!orbit_metadata_get_glyph_dsc(&font,NULL,'A',0));
    font.dsc=NULL;assert(!orbit_metadata_get_bitmap(&font,'A'));
    puts("PASS: all four cmap formats, exclusive endpoints/gaps, corrupt IDs/bitmap spans, empty glyphs and invalid inputs fail safely");
}
int main(void) {
    bounds_test();
    unsigned largest=0,decoded=0,latin=0;
    const lv_font_t *fonts[]={&orbit_metadata_24,&orbit_metadata_12,&orbit_metadata_latin_24,&orbit_metadata_latin_14};
    const uint32_t samples[]={0x5e78,0x798f,0x7684,0x5c3a,0x5ea6,0x7af9,0x5185,0x307e,0x308a,0x3084,
        0x305b,0x306e,0x3082,0x3055,0x3057,0x97f3,0x6a02,0x30df,0x30e5,0x30fc,0x30b8,0x30c3,0x30af,
        0x178,0x152,0x153,0x1e9e,0xa1,0xbf,0x2019,0x201c,0x201d,0x2013,0x2014,0x2026,0x20ac,0x3a9,0x416};
    for(unsigned size=0;size<2;++size) {
        lv_font_t primary;uint8_t top=orbit_metadata_prepare_font(&primary,size!=0);
        const lv_font_t *old=size?&lv_font_montserrat_14:&lv_font_montserrat_24;
        assert(primary.line_height-primary.base_line-top==old->line_height-old->base_line);
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(&primary,&glyph,'A',0) && glyph.resolved_font==&primary);
        for(uint32_t cp=0xa0;cp<=0x17f;++cp) {
            assert(lv_font_get_glyph_dsc(&primary,&glyph,cp,0) && !glyph.is_placeholder);
            if(glyph.box_w && glyph.box_h) {
                int y=primary.line_height-primary.base_line-glyph.box_h-glyph.ofs_y;
                assert(y>=0 && y+glyph.box_h<=primary.line_height);
                assert(lv_font_get_glyph_bitmap(glyph.resolved_font,cp));++latin;
            }
        }
        for(unsigned i=0;i<sizeof(samples)/sizeof(samples[0]);++i) {
            assert(lv_font_get_glyph_dsc(&primary,&glyph,samples[i],0));
            assert(!glyph.is_placeholder && glyph.box_w && glyph.box_h);
            assert(lv_font_get_glyph_bitmap(glyph.resolved_font,samples[i]));
            int y=primary.line_height-primary.base_line-glyph.box_h-glyph.ofs_y;
            assert(y>=0 && y+glyph.box_h<=primary.line_height);
        }
        printf("PASS: %s metadata Latin1/ExtendedA, ligatures, sharpS, punctuation, CJK/Greek/Cyrillic samples fit line box; top=%u, height=%d, baseline=%d\n",size?"artist":"title",top,primary.line_height,primary.base_line);
    }
    // Query all BMP gaps/endpoints using the actual strict reader for all4 fonts.
    for(unsigned i=0;i<4;++i)for(uint32_t cp=0x80;cp<=0xffef;++cp) {
        lv_font_glyph_dsc_t glyph;
        if(!fonts[i]->get_glyph_dsc(fonts[i],&glyph,cp,0) || !glyph.box_w || !glyph.box_h)continue;
        unsigned bytes=(glyph.box_w*glyph.box_h*glyph.bpp+7)/8;if(bytes>largest)largest=bytes;
        assert(bytes<=512);const uint8_t *bitmap=lv_font_get_glyph_bitmap(fonts[i],cp);assert(bitmap);
        volatile uint8_t last=bitmap[bytes-1];(void)last;++decoded;
    }
    printf("PASS: %u Latin ink boxes, %u font glyph reads/all BMP gaps, largest glyph%u bytes in Flash\n",latin,decoded,largest);
    // Golden public metrics and every bitmap byte from the original unpacked fonts.
    // Include absent/empty glyphs and cmap gaps, not only visible samples.
    const char *names[]={"orbit_metadata_24","orbit_metadata_12","orbit_metadata_latin_24","orbit_metadata_latin_14"};
    for(unsigned i=0;i<4;++i) {
        uint64_t hash=UINT64_C(14695981039346656037);unsigned count=0;
        for(uint32_t cp=0;cp<=0xffff;++cp) {
            lv_font_glyph_dsc_t g={0};
            bool found=fonts[i]->get_glyph_dsc(fonts[i],&g,cp,0);
            hash=mix(mix(hash,cp),found);
            if(!found) continue;
            ++count;hash=mix(hash,g.adv_w);hash=mix(hash,g.box_w);hash=mix(hash,g.box_h);
            hash=mix(hash,(uint32_t)(int32_t)g.ofs_x);hash=mix(hash,(uint32_t)(int32_t)g.ofs_y);
            hash=mix(hash,g.bpp);hash=mix(hash,g.is_placeholder);
            unsigned bytes=(g.box_w*g.box_h*g.bpp+7)/8;
            const uint8_t *bits=fonts[i]->get_glyph_bitmap(fonts[i],cp);
            assert(bits);
            for(unsigned j=0;j<bytes;++j) hash=mix(hash,bits[j]);
        }
        printf("FONT_FINGERPRINT %s %016" PRIx64 " %u\n",names[i],hash,count);
    }
    const uint32_t numeric[]={0x20,0x2b,0x2c,0x2d,0x2e,'0','1','2','3','4','5','6','7','8','9','C','F',0xb0};
    assert(orbit_numeric_48.line_height==lv_font_montserrat_48.line_height);
    assert(orbit_numeric_48.base_line==lv_font_montserrat_48.base_line);
    assert(orbit_numeric_48.underline_position==lv_font_montserrat_48.underline_position);
    assert(orbit_numeric_48.underline_thickness==lv_font_montserrat_48.underline_thickness);
    for(unsigned i=0;i<sizeof numeric/sizeof *numeric;++i) {
        for(unsigned j=0;j<sizeof numeric/sizeof *numeric+2;++j) {
            lv_font_glyph_dsc_t a={0},b={0};uint32_t cp=numeric[i],next=j<2?(j?10:0):numeric[j-2];
            assert(lv_font_get_glyph_dsc(&lv_font_montserrat_48,&a,cp,next));
            assert(lv_font_get_glyph_dsc(&orbit_numeric_48,&b,cp,next));
            assert(a.adv_w==b.adv_w && a.box_w==b.box_w && a.box_h==b.box_h);
            assert(a.ofs_x==b.ofs_x && a.ofs_y==b.ofs_y && a.bpp==b.bpp && a.is_placeholder==b.is_placeholder);
            unsigned bytes=(a.box_w*a.box_h*a.bpp+7)/8;
            const uint8_t *old=lv_font_get_glyph_bitmap(&lv_font_montserrat_48,cp);
            const uint8_t *now=lv_font_get_glyph_bitmap(&orbit_numeric_48,cp);
            for(unsigned k=0;k<bytes;++k) assert(old[k]==now[k]);
        }
    }
    printf("PASS: numeric subset all 360 character pairs/end markers, kerning, pixels and line extents equal original full48px font\n");
    for(uint32_t cp=0;cp<=0xffff;++cp) {
        bool expected=cp==9; // LVGL aliases a tab to its space glyph.
        for(unsigned i=0;i<sizeof numeric/sizeof *numeric;++i)if(cp==numeric[i])expected=true;
        lv_font_glyph_dsc_t out={0};
        assert(orbit_numeric_48.get_glyph_dsc(&orbit_numeric_48,&out,cp,0)==expected);
    }
    const uint32_t outside[]={0x10000,0x10ffff,UINT32_MAX};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<sizeof outside/sizeof *outside;++j) {
        lv_font_glyph_dsc_t out={0};
        assert(!fonts[i]->get_glyph_dsc(fonts[i],&out,outside[j],0));
        assert(!fonts[i]->get_glyph_bitmap(fonts[i],outside[j]));
    }
    puts("PASS: numeric BMP coverage exactly 18 retained characters plus LVGL tab alias; supplementary/invalid metadata codepoints safe");
    _lv_font_clean_up_fmt_txt();
}
