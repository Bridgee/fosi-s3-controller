#include "weather_icon.h"
#include "weather_assets.h"
#include <string.h>
#include <stdlib.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif
namespace orbit_weather {
namespace {
uint8_t *pixels=nullptr;lv_img_dsc_t current={};Icon previous=Icon::Count;
bool previousLight=false,previousDay=true;
}
const lv_img_dsc_t *display_icon(Icon icon,bool light,bool day) {
    const auto *source=icon_image(icon);const auto *palette=icon_palette(icon,light,day);if(!source || !palette)return nullptr;
    if(!pixels){
#if defined(ARDUINO_ARCH_ESP32)
        pixels=static_cast<uint8_t*>(heap_caps_malloc(4672,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
        pixels=static_cast<uint8_t*>(malloc(4672));
#endif
        if(!pixels)return source;
        current=*source;current.data=pixels;
    }
    if(previous!=icon || previousLight!=light || previousDay!=day){
        lv_img_cache_invalidate_src(&current);
        if(previous!=icon)memcpy(pixels,source->data,source->data_size);
        memcpy(pixels,palette,64);previous=icon;previousLight=light;previousDay=day;
    }
    return &current;
}
void draw_icon(lv_event_t *event,Icon icon,bool light,bool day) {
    const auto *image=display_icon(icon,light,day);if(!image)return;
    auto *object=lv_event_get_target(event);auto *ctx=lv_event_get_draw_ctx(event);
    lv_area_t area;lv_obj_get_coords(object,&area);lv_draw_img_dsc_t dsc;lv_draw_img_dsc_init(&dsc);
    lv_obj_init_draw_img_dsc(object,LV_PART_MAIN,&dsc);dsc.recolor_opa=LV_OPA_TRANSP;lv_draw_img(ctx,&dsc,&area,image);
}
}
