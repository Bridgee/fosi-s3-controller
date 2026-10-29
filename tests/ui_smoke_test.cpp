#include "intro_ai_runtime.h"
#include "intro_async_platform.h"
#include "orbit_ui.h"
#include "firmware_version.h"
#include "iphone_trigger.h"
namespace orbit_iphone {
LaunchState fixture=LaunchState::Ready;unsigned sends=0;
LaunchState state(){return fixture;}
bool start(){++sends;fixture=LaunchState::Sending;return true;}
}

#include "weather.h"
#include "weather_assets.h"
#include "weather_model.h"
namespace orbit_weather {static Reading fixture;static unsigned reads=0;Reading read(){++reads;return fixture;}
Reading trusted_read(){return fixture;}}
#include <stdlib.h>
#include "lyrics.h"
#include "lyrics_parser.h"
namespace orbit_lyrics {
static uint32_t generation=1;static Document *next=nullptr;static bool pending=false;
uint32_t observe(const s3::Snapshot&){return generation;}
bool receive(Result &r){if(!pending)return false;r.generation=generation;r.document=next;next=nullptr;pending=false;return true;}
void release(Document *d){free(d);}
}
#include "network_clock.h"
#include "pixel_clock_font.h"
#include "time_tools.h"
#include "weather_icon.h"
#include <math.h>
#include "ui_themes.h"
#include "appearance_settings.h"
#include <Preferences.h>
static uint8_t testedBrightness=12;
void orbit_display_set_brightness(uint8_t value){testedBrightness=value;}
uint8_t orbit_display_brightness(){return testedBrightness;}
#include "touch_regions.h"
#include "s3_client.h"
#include "status_icons.h"
#include "source_icons.h"
#include "source_status.h"
#include "artwork_decoder.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <vector>

// Test-only QR allocator fault injection; production LVGL/firmware are untouched.
static unsigned qr_fault_mask=0,qr_fault_count=0,qr_live_count=0,qr_live_peak=0;
static bool qr_track=false;static void *qr_allocations[4]={};
extern "C" void *orbit_test_lv_alloc(size_t size){
    if(qr_fault_mask && size==822){const unsigned index=qr_fault_count++;if(index<2 && (qr_fault_mask&(1u<<index)))return nullptr;}
    void *p=malloc(size);
    if(p && qr_track && size==822){unsigned i=0;while(i<4 && qr_allocations[i])++i;assert(i<4);qr_allocations[i]=p;++qr_live_count;if(qr_live_count>qr_live_peak)qr_live_peak=qr_live_count;}
    return p;
}
extern "C" void orbit_test_lv_free(void *p){
    if(p)for(auto &entry:qr_allocations)if(entry==p){entry=nullptr;assert(qr_live_count);--qr_live_count;break;}
    free(p);
}

namespace s3 {
static Artwork nextArtwork;static bool artworkPending=false,bt_requested=false;
static Snapshot next;static bool pending=false,accept=false;static unsigned writes=0,volume_writes=0;static Action last_action=Action::Volume;static int last_value=0;
void begin() {}
void requestBluetoothInfo(bool wanted) {bt_requested=wanted;}
bool receive(Snapshot &s) {if(!pending)return false;s=next;pending=false;return true;}
bool receiveArtwork(Artwork &a) {if(!artworkPending)return false;a=nextArtwork;nextArtwork=Artwork{};artworkPending=false;return true;}
bool send(Action a,int value) {++writes;last_action=a;last_value=value;if(a==Action::Volume)++volume_writes;return accept;}
}
namespace orbit_clock {
static Reading fixture;static unsigned reads=0,hookReads=0;static Reading (*trustedHook)()=nullptr;
void begin() {}
Reading read(){++reads;return fixture;}
Reading trusted_read(){return trustedHook?trustedHook():fixture;}
}
static lv_color_t pixels[480*40],frame[480*480];
static unsigned flush_count=0;
static uint32_t clock_pixels(){uint32_t hash=2166136261u;for(const auto &p:frame){hash^=p.full;hash*=16777619u;}return hash;}
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *colors) {
    ++flush_count;
    assert(area->x1>=0 && area->y1>=0 && area->x2<480 && area->y2<480);
    for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++)
        frame[y*480+x]=*colors++;
    lv_disp_flush_ready(driver);
}
static bool visible(lv_obj_t *o) {
    for(;o;o=lv_obj_get_parent(o)) if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return false;
    return true;
}
static lv_obj_t *button(lv_obj_t *o,const char *prefix) {
    if(!visible(o))return nullptr;
    if(lv_obj_check_type(o,&lv_btn_class) && lv_obj_get_child_cnt(o)) {
        auto *label=lv_obj_get_child(o,0);
        if(lv_obj_check_type(label,&lv_label_class) && !strncmp(lv_label_get_text(label),prefix,strlen(prefix)))return o;
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);i++) {
        auto *b=button(lv_obj_get_child(o,i),prefix);if(b)return b;
    }
    return nullptr;
}
static lv_obj_t *footer_image(lv_obj_t *o) {
    auto *parent=lv_obj_get_parent(o);
    if(lv_obj_check_type(o,&lv_img_class) && parent && lv_obj_get_width(parent)==30 && lv_obj_get_height(parent)==24)return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)if(auto *a=footer_image(lv_obj_get_child(o,i)))return a;
    return nullptr;
}
static lv_obj_t *footer_target(lv_obj_t *o) {
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_width(o)==80 && lv_obj_get_height(o)==40)return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)if(auto *t=footer_target(lv_obj_get_child(o,i)))return t;
    return nullptr;
}
static bool footer_visible() {
    auto *image=footer_image(lv_scr_act()),*target=footer_target(lv_scr_act());assert(image && target);
    assert(visible(image)==visible(target));
    return visible(image);
}
static bool footer_is(orbit_status::Icon icon) {
    const void *source=lv_img_get_src(footer_image(lv_scr_act()));
    for(unsigned i=0;i<(icon==orbit_status::Icon::Waiting?orbit_status::FrameCount:1);++i)
        if(source==orbit_status::icon_image(icon,i))return true;
    return false;
}
static lv_obj_t *image_of_size(lv_obj_t *o,int side,bool require_visible=true) {
    if(require_visible && !visible(o))return nullptr;
    if(lv_obj_check_type(o,&lv_img_class) && lv_obj_get_width(o)==side && lv_obj_get_height(o)==side)return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)if(auto *a=image_of_size(lv_obj_get_child(o,i),side,require_visible))return a;
    return nullptr;
}
static s3::Artwork test_artwork(unsigned revision,const char *url) {
    s3::Artwork a;a.data=static_cast<uint8_t*>(malloc(s3::ArtworkBytes));assert(a.data);
    a.size=s3::ArtworkBytes;a.large=static_cast<uint8_t*>(malloc(s3::ArtworkLargeBytes));assert(a.large);a.largeSize=s3::ArtworkLargeBytes;a.revision=revision;snprintf(a.url,sizeof(a.url),"%s",url);
    auto *pixel=reinterpret_cast<uint16_t*>(a.data);
    for(unsigned i=0;i<s3::ArtworkSide*s3::ArtworkSide;++i)pixel[i]=0xf800;
    auto *large=reinterpret_cast<uint16_t*>(a.large);
    for(unsigned i=0;i<s3::ArtworkLargeSide*s3::ArtworkLargeSide;++i)large[i]=0x07ff;
    large[0]=0xf800;large[319]=0x07e0;large[319*320]=0x001f;large[320*320-1]=0xffff;
    return a;
}
static lv_obj_t *reading_view(lv_obj_t *o){if(!visible(o))return nullptr;if(lv_obj_get_width(o)==328 && lv_obj_get_height(o)==236 && lv_obj_has_flag(o,LV_OBJ_FLAG_SCROLLABLE))return o;for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)if(auto *v=reading_view(lv_obj_get_child(o,i)))return v;return nullptr;}
static lv_obj_t *find_named_label(lv_obj_t *o,const char *value) {
    if(!visible(o))return nullptr;
    if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),value))return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)
        if(auto *found=find_named_label(lv_obj_get_child(o,i),value))return found;
    return nullptr;
}
static bool has_label(lv_obj_t *object,const char *value) {
    if(!visible(object))return false;
    if(lv_obj_check_type(object,&lv_label_class) && !strcmp(lv_label_get_text(object),value))return true;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);++i)
        if(has_label(lv_obj_get_child(object,i),value))return true;
    return false;
}
static void assert_button_labels_fit(lv_obj_t *object) {
    if(!visible(object))return;
    if(lv_obj_check_type(object,&lv_btn_class)) {
        lv_area_t parent;lv_obj_get_coords(object,&parent);
        for(uint32_t i=0;i<lv_obj_get_child_cnt(object);++i) {
            auto *child=lv_obj_get_child(object,i);
            if(!lv_obj_check_type(child,&lv_label_class))continue;
            lv_area_t bounds;lv_obj_get_coords(child,&bounds);
            assert(bounds.x1>=parent.x1 && bounds.y1>=parent.y1);
            if(bounds.x2>parent.x2 || bounds.y2>parent.y2)
                fprintf(stderr,"Label overflow: %s (%d,%d..%d,%d) in (%d,%d..%d,%d)\n",lv_label_get_text(child),bounds.x1,bounds.y1,bounds.x2,bounds.y2,parent.x1,parent.y1,parent.x2,parent.y2);
            assert(bounds.x2<=parent.x2 && bounds.y2<=parent.y2);
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);++i)
        assert_button_labels_fit(lv_obj_get_child(object,i));
}
// Verify rendered content, not a copy of the layout arithmetic.
static void assert_full_single_line(lv_obj_t *label) {
    const char *value=lv_label_get_text(label);
    const auto *font=lv_obj_get_style_text_font(label,0);
    assert(!strchr(value,'\n'));
    if(lv_txt_get_width(value,strlen(value),font,0,LV_TEXT_FLAG_NONE)>lv_obj_get_width(label))fprintf(stderr,"Full label clipped: %s natural=%d available=%d\n",value,int(lv_txt_get_width(value,strlen(value),font,0,LV_TEXT_FLAG_NONE)),int(lv_obj_get_width(label)));
    assert(lv_txt_get_width(value,strlen(value),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(label));
    assert(lv_obj_get_height(label)==font->line_height);
}
static void assert_choice_content(lv_obj_t *b,const char *detail=nullptr) {
    auto *title=lv_obj_get_child(b,0);assert_full_single_line(title);
    assert(lv_obj_get_style_text_font(title,0)==&lv_font_montserrat_14);
    auto *icon=lv_obj_get_child(b,1);
    const int middle=lv_obj_get_height(b);
    assert(abs(2*lv_obj_get_y(icon)+lv_obj_get_height(icon)-middle)<=1);
    assert(lv_obj_get_x(title)>lv_obj_get_x(icon)+lv_obj_get_width(icon));
    int bottom=lv_obj_get_y(title)+lv_obj_get_height(title);
    if(detail) {
        auto *sub=lv_obj_get_child(b,2);assert(!strcmp(lv_label_get_text(sub),detail));
        assert_full_single_line(sub);assert(lv_obj_get_style_text_font(sub,0)==&lv_font_montserrat_12);
        assert(lv_obj_get_x(sub)==lv_obj_get_x(title));
        assert(lv_obj_get_y(sub)>bottom);bottom=lv_obj_get_y(sub)+lv_obj_get_height(sub);
    }
    assert(abs(lv_obj_get_y(title)+bottom-middle)<=1);
}
static void assert_back_content() {
    auto *b=button(lv_scr_act(),"Back");assert(b && lv_obj_get_child_cnt(b)==2);
    auto *title=lv_obj_get_child(b,0),*icon=lv_obj_get_child(b,1);
    assert_full_single_line(title);assert_full_single_line(icon);
    assert(abs(lv_obj_get_x(icon)+lv_obj_get_x(title)+lv_obj_get_width(title)-lv_obj_get_width(b))<=1);
    assert(abs(2*lv_obj_get_y(title)+lv_obj_get_height(title)-lv_obj_get_height(b))<=1);
    assert(abs(2*lv_obj_get_y(icon)+lv_obj_get_height(icon)-lv_obj_get_height(b))<=1);
    assert(lv_obj_get_x(title)-lv_obj_get_x(icon)-lv_obj_get_width(icon)==8);
    assert(lv_obj_get_style_text_color(title,0).full==lv_obj_get_style_text_color(icon,0).full);
}
static void assert_side_content(lv_obj_t *b) {
    auto *icon=lv_obj_get_child(b,0),*name=lv_obj_get_child(b,1);
    assert_full_single_line(name);
    assert(abs(2*lv_obj_get_x(icon)+lv_obj_get_width(icon)-lv_obj_get_width(b))<=1);
    assert(abs(lv_obj_get_y(icon)+lv_obj_get_y(name)+lv_obj_get_height(name)-lv_obj_get_height(b))<=1);
}
static void collect_buttons(lv_obj_t *o,std::vector<lv_obj_t *> &out) {
    if(!visible(o))return;
    const bool footer=lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_width(o)==80 && lv_obj_get_height(o)==40;
    if((lv_obj_check_type(o,&lv_btn_class) || footer) && !lv_obj_has_state(o,LV_STATE_DISABLED))out.push_back(o);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)collect_buttons(lv_obj_get_child(o,i),out);
}
static void assert_touch_targets() {
    std::vector<lv_obj_t *> buttons;collect_buttons(lv_scr_act(),buttons);
    for(auto *b:buttons) {
        lv_area_t a;lv_obj_get_click_area(b,&a);
        if(lv_obj_get_width(b)==80 && lv_obj_get_height(b)==40)
            assert(lv_area_get_width(&a)>=80 && lv_area_get_height(&a)>=40); // Physical bottom strip.
        else assert(lv_area_get_width(&a)>=60 && lv_area_get_height(&a)>=60);
    }
    // Check actual hit tests, including trimmed side targets, at every overlap pixel.
    for(size_t i=0;i<buttons.size();++i)for(size_t j=i+1;j<buttons.size();++j) {
        lv_area_t a,b,c;lv_obj_get_click_area(buttons[i],&a);lv_obj_get_click_area(buttons[j],&b);
        if(!_lv_area_intersect(&c,&a,&b))continue;
        for(int y=c.y1;y<=c.y2;++y)for(int x=c.x1;x<=c.x2;++x) {
            lv_point_t p={lv_coord_t(x),lv_coord_t(y)};
            assert(!(lv_obj_hit_test(buttons[i],&p) && lv_obj_hit_test(buttons[j],&p)));
        }
    }
}
static void advance(unsigned ticks=100) {
    for(unsigned i=0;i<ticks/10;i++) {lv_tick_inc(10);orbit_intro_runtime::testPump();lv_timer_handler();}
}
static lv_point_t pointer_point={0,0};
static lv_indev_state_t pointer_state=LV_INDEV_STATE_REL;
static void pointer_read(lv_indev_drv_t *,lv_indev_data_t *data) {
    data->point=pointer_point;orbit_ui_touch_sample(pointer_point.x,pointer_point.y,pointer_state==LV_INDEV_STATE_PR);data->state=pointer_state;
}
static void pointer(int x,int y,bool pressed,unsigned ticks=80) {
    pointer_point={lv_coord_t(x),lv_coord_t(y)};
    pointer_state=pressed?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;advance(ticks);
}
static void click(const char *prefix) {
    auto *b=button(lv_scr_act(),prefix);if(!b)fprintf(stderr,"Missing visible button: %s\n",prefix);assert(b);
    assert(!lv_obj_has_state(b,LV_STATE_DISABLED));
    lv_event_send(b,LV_EVENT_CLICKED,nullptr);advance();
}
static void assert_canvas_edge() {
    // Even pixels outside the nominal circular aperture must be dark theme BG,
    // so an aperture/raster mismatch cannot expose a light LVGL screen/fringe.
    const auto background=(button(lv_scr_act(),"TIME") || button(lv_scr_act(),"STOP"))?lv_color_hex(0).full:lv_obj_get_style_bg_color(lv_scr_act(),0).full;
    for(int i=0;i<480;++i) {
        assert(frame[i].full==background && frame[479*480+i].full==background);
        assert(frame[i*480].full==background && frame[i*480+479].full==background);
    }
}
static void screenshot(const char *path) {
    lv_obj_invalidate(lv_scr_act());advance();
    assert_button_labels_fit(lv_scr_act());assert_touch_targets();assert_canvas_edge();
    if(strstr(path,"volume")) {
        unsigned bright=0;
        for(int y=164;y<260;++y)for(int x=140;x<340;++x) {
            lv_color32_t c;c.full=lv_color_to32(frame[y*480+x]);
            if(c.ch.red>180 && c.ch.green>170 && c.ch.blue>150)++bright;
        }
        assert(bright>100); // Read-only value must actually render, not only exist as a label.
    }
    if(auto *back=button(lv_scr_act(),"Back")) {
        lv_area_t area;lv_obj_get_coords(lv_obj_get_child(back,0),&area);unsigned bright=0;
        for(int y=area.y1;y<=area.y2;++y)for(int x=area.x1;x<=area.x2;++x) {
            lv_color32_t c;c.full=lv_color_to32(frame[y*480+x]);
            if(c.ch.red>180 && c.ch.green>170 && c.ch.blue>150)++bright;
        }
        assert(bright>10); // Back must remain visible after page changes/full redraw.
    }
    FILE *file=fopen(path,"wb");assert(file);
    fprintf(file,"P6\n480 480\n255\n");
    for(auto color:frame) {
        lv_color32_t rgb;rgb.full=lv_color_to32(color);
        unsigned char bytes[]={rgb.ch.red,rgb.ch.green,rgb.ch.blue};
        fwrite(bytes,1,3,file);
    }
    fclose(file);
}
static unsigned record_shine_hash() {
    unsigned hash=2166136261u;
    for(int y=110;y<265;++y)for(int x=163;x<318;++x) {
        const int dx=x-240,dy=y-187,r=dx*dx+dy*dy;
        if(r<36*36 || r>78*78)continue;
        hash=(hash^frame[y*480+x].full)*16777619u;
    }
    return hash;
}
static unsigned arm_ink(bool parked) {
    unsigned ink=0;const int x0=parked?342:281,y0=parked?211:205;
    for(int y=y0;y<y0+20;++y)for(int x=x0;x<x0+16;++x) {
        lv_color32_t c;c.full=lv_color_to32(frame[y*480+x]);
        if(c.ch.red>150 && c.ch.green>120 && c.ch.blue>90)++ink;
    }
    return ink;
}
static void assert_clean_center() {
    // Exclude spindle and rim: the former lettering area is now a flat label.
    bool sampled=false;uint16_t reference=0;
    for(int y=163;y<211;y++)for(int x=216;x<264;x++) {
        float dx=x-239.5f,dy=y-186.5f,r2=dx*dx+dy*dy;
        if(r2<8*8 || r2>23*23)continue;
        if(!sampled){reference=frame[y*480+x].full;sampled=true;}
        assert(frame[y*480+x].full==reference);
    }
    assert(sampled);
}

int main(int argc,char **argv) {
    static_assert(orbit_status::PixelBytes==10944,"Review footer Flash budget when assets change");
    std::vector<unsigned> hashes;
    for(auto icon:{orbit_status::Icon::Connected,orbit_status::Icon::Waiting,orbit_status::Icon::Confirmed,orbit_status::Icon::Offline,orbit_status::Icon::Limited,orbit_status::Icon::Error,orbit_status::Icon::Sleep,orbit_status::Icon::Demo}) {
        for(unsigned f=0;f<(icon==orbit_status::Icon::Waiting?orbit_status::FrameCount:1);++f) {
            auto *asset=orbit_status::icon_image(icon,f);
            assert(asset->header.w==24 && asset->header.h==24 && asset->header.cf==LV_IMG_CF_ALPHA_8BIT && asset->data_size==576);
            unsigned hash=2166136261u,ink=0;
            for(unsigned i=0;i<asset->data_size;++i) {hash=(hash^asset->data[i])*16777619u;ink+=asset->data[i];}
            assert(ink>0 && ink<576*255);
            for(auto previous:hashes)assert(previous!=hash);
            hashes.push_back(hash);
        }
    }
    lv_init();
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,nullptr,480*40);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);
    driver.hor_res=driver.ver_res=480;driver.draw_buf=&buffer;driver.flush_cb=flush;
    assert(lv_disp_drv_register(&driver));
    static lv_indev_drv_t input;lv_indev_drv_init(&input);
    input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;
    assert(lv_indev_drv_register(&input));
    orbit_ui_create();advance();
    // Unknown is text-only, centered in both locations, without a blank icon slot.
    assert(s3::source_icon_image(s3::SourceIcon::Unknown)==nullptr);
    auto checkSourceCenter=[](const char *text) {
        auto *name=find_named_label(lv_scr_act(),text);assert(name);
        auto *row=lv_obj_get_parent(name);lv_area_t area;lv_obj_get_coords(name,&area);
        assert(abs(area.x1+area.x2+1-480)<=1);
        assert(lv_obj_has_flag(lv_obj_get_child(row,0),LV_OBJ_FLAG_HIDDEN));
    };
    checkSourceCenter("UNKNOWN SOURCE");click("I/O");checkSourceCenter("Unknown source");click("Back");

    auto *indicator=footer_image(lv_scr_act());assert(indicator && footer_is(orbit_status::Icon::Waiting));
    auto *frame=lv_img_get_src(indicator);advance(300);assert(lv_img_get_src(indicator)!=frame);
    // The bottom90-degree gap is not a volume entry lane. The status glyph
    // is a local About shortcut, and Back returns to the actual originating page.
    for(auto point:{lv_point_t{180,448},lv_point_t{300,448},lv_point_t{160,410}}) {
        pointer(point.x,point.y,true);pointer(point.x,point.y,false);
        assert(button(lv_scr_act(),"I/O") && !has_label(lv_scr_act(),"READ ONLY"));
    }
    pointer(240,460,true);pointer(240,460,false);
    assert(has_label(lv_scr_act(),"Screen IP") && s3::bt_requested);
    assert(!footer_visible());
    const void *hidden_frame=lv_img_get_src(indicator);advance(400);assert(lv_img_get_src(indicator)==hidden_frame);
    pointer(240,438,true);pointer(240,438,false);
    assert(button(lv_scr_act(),"I/O") && !s3::bt_requested);
    assert(footer_visible());
    click("--");assert(!footer_visible());
    pointer(240,438,true);pointer(240,438,false);assert(button(lv_scr_act(),"I/O"));
    click("I/O");assert(!footer_visible());
    assert(s3::writes==0 && s3::volume_writes==0);
    assert_button_labels_fit(lv_scr_act());
    for(const char *name:{"Bluetooth","Line in","HDMI","Optical"})assert_choice_content(button(lv_scr_act(),name));
    assert_choice_content(button(lv_scr_act(),"Analog"),"RCA + XLR");
    std::vector<lv_obj_t *> all;collect_buttons(lv_scr_act(),all);
    for(auto *b:all)if(!strcmp(lv_label_get_text(lv_obj_get_child(b,0)),"Optical"))assert_choice_content(b);
    assert_back_content();assert_side_content(button(lv_scr_act(),"i"));assert_side_content(button(lv_scr_act(),LV_SYMBOL_TINT));
    assert_touch_targets();
    // Physical connector choices use the same immutable assets as source badges.
    for(const auto &entry: {s3::SourcePresentation{"Line in",s3::SourceIcon::Line,true},
                           s3::SourcePresentation{"HDMI",s3::SourceIcon::Hdmi,true},
                           s3::SourcePresentation{"Optical",s3::SourceIcon::Optical,true},
                           s3::SourcePresentation{"Analog",s3::SourceIcon::Analog,true}}) {
        auto *choice=button(lv_scr_act(),entry.label);assert(choice);
        auto *image=image_of_size(choice,entry.icon==s3::SourceIcon::Analog?26:20);assert(image);
        assert(lv_img_get_src(image)==s3::source_icon_image(entry.icon));
        assert(lv_obj_get_style_img_recolor_opa(image,0)==LV_OPA_COVER);
    }
    click("i");assert(has_label(lv_scr_act(),"Screen IP") && has_label(lv_scr_act(),"Wi-Fi offline"));
    assert_button_labels_fit(lv_scr_act());click("Back");
    assert(button(lv_scr_act(),LV_SYMBOL_TINT));
    click(LV_SYMBOL_TINT);assert_button_labels_fit(lv_scr_act()); // Also offline.
    for(const auto &palette:orbit_theme::palettes) {
        const char *name=palette.name;
        auto *b=button(lv_scr_act(),name);assert(b);
        auto *label=lv_obj_get_child(b,0),*swatch=lv_obj_get_child(b,1);assert_full_single_line(label);
        assert(abs(lv_obj_get_y(swatch)+lv_obj_get_y(label)+lv_obj_get_height(label)-lv_obj_get_height(b))<=1);
        assert(abs(2*lv_obj_get_x(swatch)+lv_obj_get_width(swatch)-lv_obj_get_width(b))<=1);
    }
    assert_back_content();assert_touch_targets();
    assert(has_label(lv_scr_act(),"Brightness") && has_label(lv_scr_act(),"75%"));
    const auto appearanceAudioWrites=s3::writes;
    click("+");assert(testedBrightness==13 && has_label(lv_scr_act(),"81%"));
    click("-");assert(testedBrightness==12 && orbit_appearance::saved());
    for(int i=12;i>1;--i)click("-");
    assert(testedBrightness==1 && lv_obj_has_state(button(lv_scr_act(),"-"),LV_STATE_DISABLED));
    for(int i=1;i<16;++i)click("+");
    assert(testedBrightness==16 && lv_obj_has_state(button(lv_scr_act(),"+"),LV_STATE_DISABLED));
    for(int i=16;i>12;--i)click("-");
    assert(s3::writes==appearanceAudioWrites && fake_preferences::state().writes>0);
    assert(!footer_visible());
    pointer(350,440,true);pointer(350,440,false);assert(button(lv_scr_act(),"Orange"));
    click("Orange");click("Back");
    auto *theme=button(lv_scr_act(),LV_SYMBOL_TINT);assert(theme);
    lv_area_t bounds;lv_obj_get_coords(theme,&bounds);
    assert(bounds.x1==380 && bounds.y1==208 && lv_obj_get_width(theme)==64 && lv_obj_get_height(theme)==64);
    assert(button(lv_scr_act(),"HDMI")); // Theme Back returns to I/O.
    click("Back");click("--");
    assert(!button(lv_scr_act(),LV_SYMBOL_TINT) && !button(lv_scr_act(),"Theme"));
    assert(lv_obj_has_state(button(lv_scr_act(),"+"),LV_STATE_DISABLED));
    click("Back");
    s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Online;
    s3::next.connected=true;s3::next.sourceKnown=s3::next.playerKnown=true;
    s3::next.volumeKnown=s3::next.muteKnown=s3::next.outputKnown=true;
    s3::next.input=s3::next.output=0;s3::next.volume=38;s3::next.volumeMax=100;s3::next.volumeLimit=52;s3::next.volumeLimitKnown=true;
    s3::next.playing=s3::next.canToggle=s3::next.canNext=true;
    s3::next.duration=258;s3::next.elapsed=84;
    s3::next.positionKnown=true;s3::next.positionMs=84000;
    s3::next.positionSampleMs=lv_tick_get();s3::next.positionSerial=1;
    s3::next.wifiConnected=true;s3::next.automaticAddress=true;s3::next.wifiRssi=-58;
    strcpy(s3::next.localIp,"192.0.2.10");strcpy(s3::next.deviceIp,"192.0.2.20");
    strcpy(s3::next.protocol,"airplay");
    strcpy(s3::next.title,"幸せのものさし · 幸福的尺度");
    strcpy(s3::next.artist,"竹内まりや · Expressions");
    strcpy(s3::next.message,"HOST SIMULATION");
    s3::pending=true;advance(500);
    assert(footer_is(orbit_status::Icon::Connected));
    assert(has_label(lv_scr_act(),"1:24"));
    advance(1000);assert(has_label(lv_scr_act(),"1:25"));
    advance(1000);assert(has_label(lv_scr_act(),"1:26"));
    assert(s3::writes==0); // Local progress requires no added device commands.
    s3::next.playing=false;s3::next.positionMs=85000;
    s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(1200);assert(has_label(lv_scr_act(),"1:25"));
    advance(1000);assert(has_label(lv_scr_act(),"1:25"));
    s3::next.playing=true;s3::next.positionMs=120000;
    s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(200);assert(has_label(lv_scr_act(),"2:00"));
    s3::next.positionMs=84000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(200);
    // Display normalization must preserve raw source and have identical real pixels.
    char savedTitle[sizeof(s3::next.title)],savedLatinArtist[sizeof(s3::next.artist)];
    snprintf(savedTitle,sizeof(savedTitle),"%s",s3::next.title);snprintf(savedLatinArtist,sizeof(savedLatinArtist),"%s",s3::next.artist);
    auto metadata_hash=[](){uint32_t h=2166136261u;for(int y=280;y<337;++y)for(int x=65;x<415;++x){h^=::frame[y*480+x].full;h*=16777619u;}return h;};
    const char *decomposedTitle="E\xcc\x81TE\xcc\x81 A\xcc\x80 PARIS — Y\xcc\x88";
    const char *decomposedArtist="Franc\xcc\xa7oise · Mun\xcc\x83oz · Straße";
    snprintf(s3::next.title,sizeof(s3::next.title),"%s",decomposedTitle);
    snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",decomposedArtist);
    const auto latinWrites=s3::writes;s3::pending=true;advance(250);
    auto *latinTitle=find_named_label(lv_scr_act(),"ÉTÉ À PARIS — Ÿ"),*latinArtist=find_named_label(lv_scr_act(),"Françoise · Muñoz · Straße");
    assert(latinTitle && latinArtist && !strcmp(s3::next.title,decomposedTitle) && !strcmp(s3::next.artist,decomposedArtist));
    const auto *latinFont=lv_obj_get_style_text_font(latinTitle,0);
    assert(lv_obj_get_y(latinTitle)+latinFont->line_height-latinFont->base_line==286+lv_font_montserrat_24.line_height-lv_font_montserrat_24.base_line);
    assert(lv_obj_get_y(latinTitle)+lv_obj_get_height(latinTitle)<lv_obj_get_y(latinArtist));
    assert(lv_obj_get_y(latinArtist)+lv_obj_get_height(latinArtist)<342);
    uint32_t decomposedPixels=metadata_hash();
    snprintf(s3::next.title,sizeof(s3::next.title),"ÉTÉ À PARIS — Ÿ");snprintf(s3::next.artist,sizeof(s3::next.artist),"Françoise · Muñoz · Straße");
    s3::pending=true;advance(250);assert(metadata_hash()==decomposedPixels && s3::writes==latinWrites);
    if(argc>16)screenshot(argv[16]);
    snprintf(s3::next.title,sizeof(s3::next.title),"%s",savedTitle);snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",savedLatinArtist);s3::pending=true;advance(200);
    // Actual LVGL scrolling, including repeated snapshots and UTF-8 fallback.
    const auto metadataWrites=s3::writes;
    char savedArtist[sizeof(s3::next.artist)];snprintf(savedArtist,sizeof(savedArtist),"%s",s3::next.artist);
    auto artistOffset=[](lv_obj_t *label){return reinterpret_cast<lv_label_t*>(label)->offset.x;};
    for(const char *longArtist:{
        "Northline Ensemble featuring The Metropolitan Chamber Orchestra and Very Special Guests / Live Sessions",
        "竹内まりや · 幸福的尺度 · 東京交響楽団 · Expressions / Special Anniversary Orchestra and Friends"}) {
        snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",longArtist);
        s3::pending=true;advance(800);
        auto *artistLabel=find_named_label(lv_scr_act(),longArtist);assert(artistLabel);
        assert(lv_label_get_long_mode(artistLabel)==LV_LABEL_LONG_SCROLL_CIRCULAR);
        assert(lv_obj_get_x(artistLabel)==65 && lv_obj_get_width(artistLabel)==350);
        const auto *font=lv_obj_get_style_text_font(artistLabel,0);
        assert(lv_obj_get_y(artistLabel)+font->line_height-font->base_line==318+lv_font_montserrat_14.line_height-lv_font_montserrat_14.base_line);
        int before=artistOffset(artistLabel);assert(before<0);
        // An unchanged device snapshot must continue the scroll, not reset it.
        s3::pending=true;advance(400);assert(artistOffset(artistLabel)<before);
        assert(!strcmp(lv_label_get_text(artistLabel),longArtist));
        if(strstr(longArtist,"Northline")){
            const char *nfdLong="Ensemble Cafe\xcc\x81 featuring The Metropolitan Chamber Orchestra and Very Special Guests / Live Sessions";
            const char *nfcLong="Ensemble Café featuring The Metropolitan Chamber Orchestra and Very Special Guests / Live Sessions";
            snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",nfdLong);s3::pending=true;advance(600);
            auto *canonical=find_named_label(lv_scr_act(),nfcLong);assert(canonical);int offset=artistOffset(canonical);assert(offset<0);
            snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",nfcLong);s3::pending=true;advance(300);assert(artistOffset(canonical)<offset);
        }
        // While dark, even long-label animations cannot trigger display flushes.
        lv_disp_enable_invalidation(lv_disp_get_default(),false);
        orbit_ui_set_screen_awake(false);lv_timer_pause(lv_disp_get_default()->refr_timer);
        unsigned darkFlushes=flush_count;advance(500);assert(flush_count==darkFlushes);
        lv_disp_enable_invalidation(lv_disp_get_default(),true);
        orbit_ui_set_screen_awake(true);lv_timer_resume(lv_disp_get_default()->refr_timer);advance(100);
    }
    strcpy(s3::next.artist,"Northline");s3::pending=true;advance(200);
    auto *shortArtist=find_named_label(lv_scr_act(),"Northline");assert(shortArtist && artistOffset(shortArtist)==0);
    advance(1500);assert(artistOffset(shortArtist)==0 && s3::writes==metadataWrites);
    snprintf(s3::next.artist,sizeof(s3::next.artist),"%s",savedArtist);s3::pending=true;advance(200);
    assert(!has_label(lv_scr_act(),"HOST SIMULATION")); // Verbose text moved to About.
    for(auto link:{s3::LinkState::Waiting,s3::LinkState::Searching,s3::LinkState::Limited,s3::LinkState::Offline,s3::LinkState::Error}) {
        s3::next.link=link;s3::pending=true;advance(500);
        if(link==s3::LinkState::Waiting || link==s3::LinkState::Searching) {
            assert(footer_is(orbit_status::Icon::Waiting));auto *frame=lv_img_get_src(indicator);advance(300);
            assert(lv_img_get_src(indicator)!=frame);
            if(link==s3::LinkState::Waiting && argc>8)screenshot(argv[8]);
        } else {
            assert(footer_is(link==s3::LinkState::Limited?orbit_status::Icon::Limited:link==s3::LinkState::Offline?orbit_status::Icon::Offline:orbit_status::Icon::Error));
        }
    }
    s3::next.link=s3::LinkState::Ready;s3::pending=true;advance(500);
    // UI feedback only: two mock Mute queue attempts, no actual network calls.
    s3::accept=true;click(LV_SYMBOL_VOLUME_MID);
    assert(s3::last_action==s3::Action::Mute && footer_is(orbit_status::Icon::Waiting));
    s3::pending=true;advance(500);assert(footer_is(orbit_status::Icon::Waiting)); // Earlier ready poll cannot immediately erase waiting.
    s3::next.link=s3::LinkState::Waiting;s3::pending=true;advance(200);
    s3::next.link=s3::LinkState::Ready;s3::next.muted=true;s3::pending=true;advance(200);
    assert(footer_is(orbit_status::Icon::Confirmed));advance(1000);assert(footer_is(orbit_status::Icon::Connected));
    s3::next.muted=false;s3::pending=true;advance(200);
    s3::accept=false;click(LV_SYMBOL_VOLUME_MID);
    assert(footer_is(orbit_status::Icon::Error));advance(3200);
    assert(footer_is(orbit_status::Icon::Connected));
    assert(s3::writes==2 && s3::volume_writes==0);s3::writes=0;
    assert_clean_center();
    auto *home_mute=button(lv_scr_act(),LV_SYMBOL_VOLUME_MID);assert(home_mute);
    // Extended mute target remains separate from the outer-ring lane.
    lv_point_t near_mute={440,239},on_ring={466,240};
    assert(lv_indev_search_obj(lv_scr_act(),&near_mute)==home_mute);
    assert(lv_indev_search_obj(lv_scr_act(),&on_ring)!=home_mute);
    s3::next.muted=true;s3::pending=true;advance(500);
    assert(has_label(home_mute,LV_SYMBOL_MUTE));
    assert(lv_obj_get_child_cnt(home_mute)==2); // Speaker and slash, no new button caption.
    assert(lv_obj_get_style_bg_opa(home_mute,0)==LV_OPA_TRANSP);
    assert(lv_obj_get_style_border_width(home_mute,0)==0);
    assert(has_label(lv_scr_act(),LV_SYMBOL_MUTE"  MUTED"));
    assert(!lv_obj_has_flag(lv_obj_get_child(home_mute,1),LV_OBJ_FLAG_HIDDEN));
    assert_button_labels_fit(lv_scr_act());
    click(LV_SYMBOL_MUTE"  MUTED");
    auto *unmute=button(lv_scr_act(),"Unmute");assert(unmute);assert_button_labels_fit(lv_scr_act());
    assert(lv_obj_get_style_radius(unmute,0)==LV_RADIUS_CIRCLE);
    assert(lv_obj_get_width(unmute)==72 && lv_obj_get_height(unmute)==72);
    const auto *mute_font=lv_obj_get_style_text_font(lv_obj_get_child(unmute,0),0);
    assert(mute_font==&lv_font_montserrat_12);
    assert(72-lv_txt_get_width("Unmute",6,mute_font,0,LV_TEXT_FLAG_NONE)>=18);
    auto mute_background=lv_obj_get_style_bg_color(unmute,0).full;
    if(argc>7)screenshot(argv[7]);
    click("Back");
    if(argc>6)screenshot(argv[6]);
    s3::next.muted=false;s3::pending=true;advance(500);
    assert(has_label(home_mute,LV_SYMBOL_VOLUME_MID));
    assert(lv_obj_has_flag(lv_obj_get_child(home_mute,1),LV_OBJ_FLAG_HIDDEN));
    assert(has_label(lv_scr_act(),LV_SYMBOL_VOLUME_MID"  38"));
    // These use the real LVGL pointer path, not synthetic CLICKED events.
    // Broad radial tolerance is retained along the visible track, including
    // both diagonal endpoints; the empty bottom wedge is excluded.
    for(auto point:{lv_point_t{240,48},lv_point_t{83,397},lv_point_t{397,397},on_ring}) {
        pointer(point.x,point.y,true);
        assert(button(lv_scr_act(),"I/O") && !button(lv_scr_act(),"Back")); // Still pending, no hit action.
        pointer(432,240,true);pointer(432,240,false);
        assert(has_label(lv_scr_act(),"READ ONLY")); // No release retarget to Back.
        assert(has_label(lv_scr_act(),"38") && s3::writes==0);
        assert(lv_obj_get_scroll_x(lv_scr_act())==0 && lv_obj_get_scroll_y(lv_scr_act())==0);
        for(const char *symbol:{"-","+"}) {
            auto *b=button(lv_scr_act(),symbol);assert(b);
            assert(lv_obj_has_state(b,LV_STATE_DISABLED));
            assert(lv_obj_get_style_opa(b,0)==LV_OPA_70);
            assert(lv_obj_get_style_border_width(b,0)==1);
        }
        if(argc>5)screenshot(argv[5]);
        pointer(150,348,true);pointer(150,348,false);
        pointer(330,348,true);pointer(330,348,false);
        assert(has_label(lv_scr_act(),"38") && s3::writes==0);
        assert(lv_obj_get_scroll_x(lv_scr_act())==0 && lv_obj_get_scroll_y(lv_scr_act())==0);
        click("Back");
    }
    // Exercise the enabled volume path with the verified hardware ceiling.
    // No network calls occur: send() records mock commands only.
    s3::accept=true;s3::next.volumeCeiling=100;s3::pending=true;advance(200);
    pointer(240,48,true);pointer(240,48,false);
    assert(!has_label(lv_scr_act(),"READ ONLY"));
    assert(!lv_obj_has_state(button(lv_scr_act(),"-"),LV_STATE_DISABLED));
    assert(!lv_obj_has_state(button(lv_scr_act(),"+"),LV_STATE_DISABLED));
    assert(s3::writes==0); // Tapping the ring opens Volume without an absolute jump.
    assert(!footer_visible());
    pointer(240,460,true);pointer(240,460,false);assert(button(lv_scr_act(),"I/O"));
    pointer(240,460,true);pointer(240,460,false);assert(has_label(lv_scr_act(),"Screen IP"));
    pointer(240,438,true);pointer(240,438,false);assert(button(lv_scr_act(),"I/O"));
    pointer(240,48,true);pointer(240,48,false);assert(button(lv_scr_act(),"-"));
    assert(s3::writes==0); // Footer/Back never issue a volume or playback command.
    pointer(150,348,true);pointer(150,348,false);
    assert(s3::volume_writes==1 && s3::last_action==s3::Action::Volume && s3::last_value==37);
    pointer(330,348,true);pointer(330,348,false);
    assert(s3::volume_writes==2 && s3::last_value==38);
    s3::next.volume=38;s3::pending=true;advance(800); // Confirm the actual readback.
    assert(!footer_visible() && has_label(lv_scr_act(),"ANALOG OUT"));
    s3::next.volume=0;s3::pending=true;advance(1200);
    unsigned at_limit=s3::volume_writes;
    click("-");assert(s3::volume_writes==at_limit); // No negative or no-op request.
    s3::next.volume=52;s3::pending=true;advance(200);
    click("+");assert(s3::volume_writes==at_limit); // Never exceed the dynamic device cap.
    assert(has_label(lv_scr_act(),"LIMIT 52") && has_label(lv_scr_act(),"Volume limit reached"));
    advance(1800);assert(!has_label(lv_scr_act(),"LIMIT 52"));
    pointer(240,48,true);pointer(48,240,true);pointer(48,240,false);
    assert(s3::volume_writes>at_limit);
    assert(s3::last_value>=0 && s3::last_value<=52);
    s3::next.volume=s3::last_value;s3::pending=true;advance(1200);
    // Player ring/control overlap: a tangential drag enters Volume and issues
    // relative volume only, while a new ordinary button tap remains functional.
    click("Back");unsigned overlap_writes=s3::volume_writes;
    pointer(435,240,true);pointer(435,190,true);pointer(435,190,false);
    assert(button(lv_scr_act(),"Back") && s3::volume_writes>overlap_writes);
    assert(s3::last_action==s3::Action::Volume && s3::last_value<=52);
    s3::next.volume=s3::last_value;s3::pending=true;advance(1200);
    // Entering the gap while held cancels the drag. Re-entry/release neither
    // changes volume nor activates the About shortcut without a new press.
    pointer(240,48,true);unsigned before_gap=s3::volume_writes;
    pointer(240,460,true);pointer(432,240,true);pointer(432,240,false);
    assert(s3::volume_writes==before_gap && !has_label(lv_scr_act(),"Screen IP"));
    // Keep a real pointer pressed across connection loss/recovery. Motion from
    // the lost session must never become a later volume request.
    pointer(240,48,true);
    s3::next.connected=false;s3::pending=true;advance(200);
    unsigned interrupted=s3::volume_writes;pointer(48,240,true);
    s3::next.connected=true;s3::pending=true;advance(200);
    pointer(432,240,true);pointer(432,240,false);
    assert(s3::volume_writes==interrupted);
    int sent=s3::volume_writes;
    s3::next.connected=false;s3::pending=true;advance(200);
    assert(lv_obj_has_state(button(lv_scr_act(),"-"),LV_STATE_DISABLED));
    pointer(150,348,true);pointer(150,348,false);assert(s3::volume_writes==sent);
    s3::next.connected=true;s3::next.volume=38;s3::pending=true;advance(800);
    s3::accept=false;click("-");assert(s3::volume_writes==sent+1 && has_label(lv_scr_act(),"38"));
    s3::accept=true;click("+"); // Accepted local target39, then app lowers max35.
    s3::next.volumeLimit=35;s3::next.volume=35;s3::pending=true;advance(200);
    assert(has_label(lv_scr_act(),"LIMIT 35"));
    advance(2600);assert(!footer_visible() && has_label(lv_scr_act(),"ANALOG OUT"));
    sent=s3::volume_writes;click("+");assert(s3::volume_writes==sent && has_label(lv_scr_act(),"LIMIT 35"));
    s3::next.volumeLimitKnown=false;s3::pending=true;advance(1800);
    assert(lv_obj_has_state(button(lv_scr_act(),"-"),LV_STATE_DISABLED));
    assert(has_label(lv_scr_act(),"LIMIT UNAVAILABLE"));
    s3::next.volumeLimit=52;s3::next.volumeLimitKnown=true;s3::next.volume=38;
    s3::next.sourceKnown=false;s3::pending=true;advance(200);
    assert(!has_label(lv_scr_act(),"UNKNOWN SOURCE")); // Keep last input context, not control permission.
    s3::next.sourceKnown=true;s3::accept=false;
    // Restore the read-only fixture for the remaining independent regressions.
    s3::next.volumeCeiling=-1;s3::pending=true;advance(3000);
    s3::writes=s3::volume_writes=0;
    click("Back");
    if(argc>1)screenshot(argv[1]);
    click("I/O");
    if(argc>3)screenshot(argv[3]);
    click("i");
    assert(has_label(lv_scr_act(),"192.0.2.10") && has_label(lv_scr_act(),"192.0.2.20"));
    assert(has_label(lv_scr_act(),"Firmware") && has_label(lv_scr_act(),ORBIT_FIRMWARE_VERSION));
    assert(has_label(lv_scr_act(),"Strong") && !has_label(lv_scr_act(),"Uptime"));
    if(argc>4)screenshot(argv[4]);
    click("Back");assert(button(lv_scr_act(),LV_SYMBOL_TINT));
    click(LV_SYMBOL_TINT);
    for(unsigned i=0;i<100;i++) {
        unsigned index=i%orbit_theme::Count;
        const auto &palette=orbit_theme::palettes[index];
        const auto writes=s3::writes;
        if(i<orbit_theme::Count) {
            auto *b=button(lv_scr_act(),palette.name);assert(b);
            lv_area_t bounds;lv_obj_get_coords(b,&bounds);
            pointer((bounds.x1+bounds.x2)/2,(bounds.y1+bounds.y2)/2,true);
            pointer((bounds.x1+bounds.x2)/2,(bounds.y1+bounds.y2)/2,false);
        } else click(palette.name);
        assert(lv_obj_get_style_bg_color(lv_scr_act(),0).full==lv_color_hex(palette.colors[orbit_theme::BG]).full);
        for(unsigned j=0;j<orbit_theme::Count;++j) {
            auto *b=button(lv_scr_act(),orbit_theme::palettes[j].name);assert(b);
            auto *swatch=lv_obj_get_child(b,1);
            assert(lv_obj_get_style_bg_color(swatch,0).full==lv_color_hex(orbit_theme::palettes[j].swatch).full);
            assert(lv_obj_get_style_outline_width(swatch,0)==(j==index?2:0));
        }
        assert(s3::writes==writes);assert_canvas_edge();assert_touch_targets();
        if(i<orbit_theme::Count && argc>14) {
            char path[512];
            snprintf(path,sizeof(path),"%s/%s-theme.ppm",argv[14],palette.id);screenshot(path);
            click("Back");
            snprintf(path,sizeof(path),"%s/%s-routing.ppm",argv[14],palette.id);screenshot(path);
            click("Back");
            snprintf(path,sizeof(path),"%s/%s-player.ppm",argv[14],palette.id);screenshot(path);
            click("I/O");click(LV_SYMBOL_TINT);
            assert(s3::writes==writes);
        }
    }
    click("Orange");assert_button_labels_fit(lv_scr_act());
    if(argc>2)screenshot(argv[2]);
    click("Back");assert(button(lv_scr_act(),LV_SYMBOL_TINT));
    click("Back");click(LV_SYMBOL_VOLUME_MID"  38");
    assert(lv_obj_has_state(button(lv_scr_act(),"+"),LV_STATE_DISABLED)); // Ceiling remains unknown.
    assert(lv_obj_get_style_radius(button(lv_scr_act(),"Mute"),0)==LV_RADIUS_CIRCLE);
    assert(lv_obj_get_style_text_font(lv_obj_get_child(button(lv_scr_act(),"Mute"),0),0)==&lv_font_montserrat_14);
    assert(lv_obj_get_style_bg_color(button(lv_scr_act(),"Mute"),0).full==mute_background);
    if(argc>5)screenshot(argv[5]);
    click("Back");
    for(unsigned i=0;i<18;i++) {advance(1000);assert_clean_center();}
    auto *record_disc=image_of_size(lv_scr_act(),170);assert(record_disc);
    assert(!image_of_size(lv_scr_act(),52,false));
    auto shine=record_shine_hash();
    assert(arm_ink(false)>10 && arm_ink(true)<10);assert(orbit_ui_record_motion_active());
    if(argc>9)screenshot(argv[9]);
    advance(600);assert(record_shine_hash()!=shine);
    s3::next.playing=false;s3::pending=true;advance(1000);
    assert(arm_ink(true)>10 && arm_ink(false)<10);assert(!orbit_ui_record_motion_active());
    if(argc>10)screenshot(argv[10]);
    shine=record_shine_hash();
    advance(1000);assert(record_shine_hash()==shine);
    s3::next.playing=true;s3::next.artworkRevision=1;strcpy(s3::next.artwork,"/test-cover.jpg");s3::pending=true;advance(200);
    s3::nextArtwork=test_artwork(1,"/test-cover.jpg");
    auto *albumCorners=reinterpret_cast<uint16_t*>(s3::nextArtwork.data);
    albumCorners[169]=0x07e0;albumCorners[169*170]=0x001f;albumCorners[170*170-1]=0xffff;
    s3::artworkPending=true;advance(200);
    assert(!visible(record_disc));assert(!orbit_ui_record_motion_active());
    assert(arm_ink(false)==0 && arm_ink(true)==0);auto *cover=image_of_size(lv_scr_act(),170);assert(cover);
    auto *coverData=static_cast<const lv_img_dsc_t*>(lv_img_get_src(cover));assert(coverData->data_size==s3::ArtworkBytes);
    assert(reinterpret_cast<const uint16_t*>(coverData->data)[85*170+85]==0xf800);
    const auto albumViewWrites=s3::writes,albumViewVolumeWrites=s3::volume_writes;
    lv_point_t albumTop={240,103};assert(lv_indev_search_obj(lv_scr_act(),&albumTop)==cover);
    // Real pointer clicks: open without immediate dismissal; only the viewer is hit.
    for(int repeat=0;repeat<8;++repeat) {
        pointer(240,repeat==0?103:187,true,50);pointer(240,repeat==0?103:187,false,50);
        auto *moving=image_of_size(lv_scr_act(),320);assert(moving);advance(10);
        assert(lv_obj_get_style_img_opa(moving,0)>0 && lv_obj_get_style_img_opa(moving,0)<255);
        advance(300);
        auto *large=image_of_size(lv_scr_act(),320);assert(large);
        assert(!button(lv_scr_act(),"I/O") && !footer_visible());
        const auto *largeImage=static_cast<const lv_img_dsc_t*>(lv_img_get_src(large));
        assert(largeImage->header.w==320 && largeImage->header.h==320 && largeImage->data_size==204800);
        const auto *largePixels=reinterpret_cast<const uint16_t*>(largeImage->data);
        assert(largePixels[0]==0xf800 && largePixels[319]==0x07e0);
        assert(largePixels[319*320]==0x001f && largePixels[320*320-1]==0xffff);
        assert(largePixels[160*320+160]==0x07ff);
        // Both the picture and an old volume-arc point close locally, never write.
        const int closeX=repeat%2?460:240;
        pointer(closeX,240,true,50);pointer(closeX,240,false,50);advance(300);
        assert(!image_of_size(lv_scr_act(),320) && button(lv_scr_act(),"I/O"));
        assert(image_of_size(lv_scr_act(),170)==cover && s3::writes==albumViewWrites && s3::volume_writes==albumViewVolumeWrites);
    }
    // Reverse while moving, including a second reversal before returning home.
    pointer(240,187,true,50);pointer(240,187,false,50);advance(10);
    auto *moving=image_of_size(lv_scr_act(),320);assert(moving);
    int opacity=lv_obj_get_style_img_opa(moving,0);assert(opacity>0 && opacity<255);
    assert(lv_img_get_zoom(moving)==256 && lv_obj_get_x(moving)==80 && lv_obj_get_y(moving)==80);
    lv_event_send(lv_obj_get_parent(moving),LV_EVENT_CLICKED,nullptr);advance(40);
    assert(lv_obj_get_style_img_opa(moving,0)<opacity);
    lv_event_send(lv_obj_get_parent(moving),LV_EVENT_CLICKED,nullptr);advance(300);
    assert(lv_obj_get_style_img_opa(moving,0)==255);
    // Same-revision replacement must detach an open image before freeing it.
    s3::nextArtwork=test_artwork(1,"/test-cover.jpg");s3::artworkPending=true;advance(200);
    assert(!image_of_size(lv_scr_act(),320) && button(lv_scr_act(),"I/O"));
    pointer(240,187,true,40);pointer(240,187,false,40);assert(image_of_size(lv_scr_act(),320));
    orbit_ui_set_screen_awake(false);s3::pending=true;advance(400);
    orbit_ui_set_screen_awake(true);advance(100);assert(image_of_size(lv_scr_act(),320));
    // Invalidate the cover while a finger is held above the future Mute target.
    pointer(400,240,true);

    s3::next.artworkRevision=2;strcpy(s3::next.artwork,"/new-cover.jpg");s3::pending=true;advance(200);
    assert(visible(record_disc)); // Old thumbnail is dropped on a track revision.
    assert(!image_of_size(lv_scr_act(),320));
    pointer(400,240,true);pointer(400,240,false);
    assert(s3::writes==albumViewWrites && s3::volume_writes==albumViewVolumeWrites);

    s3::nextArtwork=test_artwork(1,"/test-cover.jpg");s3::artworkPending=true;advance(200);
    assert(visible(record_disc)); // Late previous-track data must not replace fallback.
    s3::next.artwork[0]=0;s3::next.artworkRevision=3;s3::pending=true;advance(200);
    shine=record_shine_hash();advance(600);assert(record_shine_hash()!=shine);
    pointer(240,187,true);pointer(240,187,false);
    assert(!image_of_size(lv_scr_act(),320) && button(lv_scr_act(),"I/O"));

    // Actual next-button path resets only after an accepted mock queue action.
    s3::next.positionKnown=true;s3::next.positionMs=84000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(200);s3::accept=true;click(LV_SYMBOL_NEXT);
    assert(has_label(lv_scr_act(),"0:00"));advance(1000);assert(has_label(lv_scr_act(),"0:00"));
    s3::next.positionMs=85000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(200);assert(has_label(lv_scr_act(),"0:00"));
    strcpy(s3::next.title,"Next host track");s3::next.positionMs=85800;
    s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(200);
    assert(has_label(lv_scr_act(),"0:00"));
    s3::next.positionMs=400;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    s3::pending=true;advance(1200);
    assert(has_label(lv_scr_act(),"0:01"));assert(s3::writes==1 && s3::last_action==s3::Action::Next);
    s3::writes=0;s3::accept=false;
    assert(s3::writes==0 && s3::volume_writes==0);
    s3::next.playerKnown=false;s3::next.playing=false;s3::next.connected=false;
    s3::next.link=s3::LinkState::Offline;s3::pending=true;advance(1000);
    assert(!orbit_ui_record_motion_active() && arm_ink(true)>10 && arm_ink(false)<10);
    shine=record_shine_hash();advance(600);assert(record_shine_hash()==shine);
    // A healthy BT audio-only snapshot uses Connected, not an exclamation.
    s3::next.connected=s3::next.sourceKnown=s3::next.playerKnown=true;
    s3::next.input=1;s3::next.duration=0;s3::next.positionKnown=false;s3::next.playing=true;
    s3::next.canToggle=s3::next.canNext=s3::next.canPrevious=false;
    s3::next.title[0]=s3::next.artist[0]=s3::next.artwork[0]=0;s3::next.link=s3::LinkState::Ready;
    s3::next.bluetooth=s3::BluetoothStatus::Connected;
    strcpy(s3::next.message,s3::connected_source_detail(s3::next));s3::pending=true;advance(800);
    assert(footer_is(orbit_status::Icon::Connected));
    assert(!has_label(lv_scr_act(),"Bluetooth audio / no track info"));
    assert(lv_obj_has_state(button(lv_scr_act(),LV_SYMBOL_PAUSE),LV_STATE_DISABLED));
    click("I/O");click("i");assert(has_label(lv_scr_act(),"Connected"));
    assert(!footer_visible());
    assert(s3::bt_requested && has_label(lv_scr_act(),"Bluetooth"));
    if(argc>11)screenshot(argv[11]);
    for(auto status:{s3::BluetoothStatus::PairingMode,s3::BluetoothStatus::Searching,
                     s3::BluetoothStatus::NotConnected,s3::BluetoothStatus::Unknown}) {
        s3::next.bluetooth=status;s3::pending=true;advance(200);
        assert(has_label(lv_scr_act(),s3::bluetooth_status_label(status)));
        assert(!footer_visible()); // BT row is independent; footer is main-only.
    }
    s3::next.bluetooth=s3::BluetoothStatus::NotConnected;s3::pending=true;advance(200);
    click("Back");assert(!s3::bt_requested);click("Back");advance(800);
    assert(!orbit_ui_record_motion_active() && arm_ink(true)>10);
    assert(lv_obj_has_state(button(lv_scr_act(),LV_SYMBOL_PLAY),LV_STATE_DISABLED));
    assert(s3::writes==0 && s3::volume_writes==0);
    for(int input=2;input<=4;++input) {
        s3::next.input=input;s3::next.playerKnown=false;s3::next.playing=false;
        s3::next.canToggle=s3::next.canPlay=s3::next.canPause=s3::next.canNext=s3::next.canPrevious=false;
        strcpy(s3::next.message,s3::connected_source_detail(s3::next));s3::pending=true;advance(800);
        assert(!has_label(lv_scr_act(),"Signal status unavailable"));
        assert(footer_is(orbit_status::Icon::Connected) && !orbit_ui_record_motion_active());
        assert(lv_obj_has_state(button(lv_scr_act(),LV_SYMBOL_PLAY),LV_STATE_DISABLED));
        click("I/O");click("i");
        assert(has_label(lv_scr_act(),s3::input_status_key(s3::next)) && has_label(lv_scr_act(),"Unknown"));
        assert(!has_label(lv_scr_act(),s3::connected_source_detail(s3::next)));
        assert_button_labels_fit(lv_scr_act());
        if(input==3 && argc>12)screenshot(argv[12]);
        if(input==4 && argc>13)screenshot(argv[13]);
        s3::next.connected=false;s3::next.link=s3::LinkState::Offline;s3::pending=true;advance(800);
        assert(has_label(lv_scr_act(),"Input") && !has_label(lv_scr_act(),input==2?"Line In signal":input==3?"HDMI signal":"Optical signal"));
        s3::next.connected=true;s3::next.link=s3::LinkState::Ready;s3::pending=true;advance(800);
        click("Back");click("Back");
    }
    assert(s3::writes==0 && s3::volume_writes==0);
    // A real power report, not pause/offline, selects the standby home scene.
    s3::next.connected=true;s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Online;
    s3::next.powerPending=s3::next.powerTransition=false;
    s3::next.sourceKnown=s3::next.playerKnown=s3::next.volumeKnown=s3::next.muteKnown=s3::next.outputKnown=true;
    s3::next.volumeLimitKnown=true;s3::next.volumeMax=s3::next.volumeCeiling=100;s3::next.volumeLimit=52;s3::next.volume=38;
    s3::next.input=s3::next.output=0;s3::next.playing=s3::next.canToggle=true;
    s3::next.canPlay=s3::next.canPause=true;
    strcpy(s3::next.title,"Power test");s3::next.artist[0]=s3::next.artwork[0]=0;
    s3::pending=true;advance(800);
    s3::accept=true;
    // An unsent/unconfirmed volume target must not survive a power transition.
    click(LV_SYMBOL_VOLUME_MID"  38");click("+");assert(has_label(lv_scr_act(),"39"));click("Back");
    const unsigned beforePower=s3::writes;
    click("I/O");auto *sleep=button(lv_scr_act(),"Standby");assert(sleep);
    lv_area_t sleepBounds;lv_obj_get_coords(sleep,&sleepBounds);
    assert(sleepBounds.x1==208 && sleepBounds.y1==14 && lv_obj_get_width(sleep)==64 && lv_obj_get_height(sleep)==64);
    assert_touch_targets();assert_button_labels_fit(lv_scr_act());
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/routing.ppm",argv[15]);screenshot(path);}
    click("Standby");assert(s3::writes==beforePower+1 && s3::last_action==s3::Action::Standby);
    assert(!button(lv_scr_act(),"Wake") && !orbit_ui_record_motion_active());
    assert(footer_is(orbit_status::Icon::Waiting));
    pointer(240,17,true);pointer(240,17,false);assert(!button(lv_scr_act(),"Back"));
    s3::next.powerTransition=true;s3::next.powerPending=true;s3::next.link=s3::LinkState::Waiting;
    s3::pending=true;advance(200);assert(!button(lv_scr_act(),"Wake"));
    s3::next.power=s3::PowerState::Standby;s3::next.powerTransition=s3::next.powerPending=false;
    s3::next.link=s3::LinkState::Ready;s3::next.playing=false;
    s3::next.title[0]=s3::next.artist[0]=s3::next.artwork[0]=0;
    s3::pending=true;advance(1200);
    auto *wake=button(lv_scr_act(),"Wake");assert(wake && !lv_obj_has_state(wake,LV_STATE_DISABLED));
    lv_area_t wakeBounds;lv_obj_get_coords(wake,&wakeBounds);
    assert(wakeBounds.x1==200 && wakeBounds.y1==200 && lv_obj_get_width(wake)==80 && lv_obj_get_height(wake)==80);
    assert(!button(lv_scr_act(),"I/O") && !button(lv_scr_act(),LV_SYMBOL_PLAY) && !orbit_ui_record_motion_active());
    assert(footer_is(orbit_status::Icon::Sleep));assert_touch_targets();
    assert(!image_of_size(lv_scr_act(),170));
    assert(!has_label(lv_scr_act(),"Standby") && !has_label(lv_scr_act(),"Wake"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/standby.ppm",argv[15]);screenshot(path);}
    auto stoppedShine=record_shine_hash();advance(2000);
    assert(record_shine_hash()==stoppedShine);
    pointer(240,17,true);pointer(240,17,false);assert(button(lv_scr_act(),"Wake"));
    pointer(240,460,true);pointer(240,460,false);
    assert(has_label(lv_scr_act(),"Power") && has_label(lv_scr_act(),"Standby"));click("Back");
    click("Wake");assert(s3::writes==beforePower+2 && s3::last_action==s3::Action::Wake);
    wake=button(lv_scr_act(),"Wake");assert(wake && lv_obj_has_state(wake,LV_STATE_DISABLED));
    assert(footer_is(orbit_status::Icon::Waiting));
    pointer(240,240,true);pointer(240,240,false);assert(s3::writes==beforePower+2);
    s3::next.power=s3::PowerState::Online;s3::next.playing=false;s3::pending=true;advance(1200);
    assert(!button(lv_scr_act(),"Wake") && button(lv_scr_act(),"I/O") && !orbit_ui_record_motion_active());
    assert(has_label(lv_scr_act(),LV_SYMBOL_VOLUME_MID"  38"));
    assert(s3::writes==beforePower+2 && footer_is(orbit_status::Icon::Connected));
    // App/remote initiated standby also leaves Volume and opens the same Wake scene.
    click(LV_SYMBOL_VOLUME_MID"  38");assert(button(lv_scr_act(),"Back"));s3::next.power=s3::PowerState::Standby;s3::pending=true;advance(800);
    assert(button(lv_scr_act(),"Wake") && !button(lv_scr_act(),"Back"));
    s3::next.connected=false;s3::next.link=s3::LinkState::Offline;s3::pending=true;advance(800);
    assert(!button(lv_scr_act(),"Wake") && footer_is(orbit_status::Icon::Offline));
    s3::next.connected=true;s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Unknown;
    s3::pending=true;advance(800);assert(!button(lv_scr_act(),"Wake"));
    click("I/O");assert(lv_obj_has_state(button(lv_scr_act(),"Standby"),LV_STATE_DISABLED));
    assert(s3::writes==beforePower+2);
    // All Back pages retain the same appearance but accept near-edge/low taps.
    s3::next.power=s3::PowerState::Online;s3::next.powerPending=s3::next.powerTransition=false;
    s3::next.muted=false;s3::next.volume=38;s3::next.volumeKnown=true;
    s3::pending=true;advance(500);click("Back");
    const auto backWrites=s3::writes,backVolumeWrites=s3::volume_writes;
    auto enterBackPage=[&](int page) {
        assert(button(lv_scr_act(),"I/O"));
        if(page==0)click(LV_SYMBOL_VOLUME_MID"  38");
        else {click("I/O");if(page==2)click(LV_SYMBOL_TINT);else if(page==3)click("i");}
        assert(button(lv_scr_act(),"Back"));assert_back_content();assert_touch_targets();
    };
    for(int page=0;page<4;++page)for(int sample=0;sample<5;++sample) {
        enterBackPage(page);auto *back=button(lv_scr_act(),"Back");
        lv_area_t bounds;lv_obj_get_coords(back,&bounds);
        assert(lv_obj_get_width(back)==152 && lv_obj_get_height(back)==52);
        assert(lv_obj_has_flag(back,LV_OBJ_FLAG_PRESS_LOCK));
        const int top=page==2?392:402,middle=438;
        lv_point_t above={240,lv_coord_t(top-2)},outside={132,479};
        assert(!lv_obj_hit_test(back,&above) && !lv_obj_hit_test(back,&outside));
        // Empty space above Back does not steal an adjacent choice or navigate.
        pointer(above.x,above.y,true,50);pointer(above.x,above.y,false,50);
        assert(button(lv_scr_act(),"Back"));
        lv_point_t p={lv_coord_t(sample==0||sample==4?134:sample==1?346:240),
                      lv_coord_t(sample==2?478:sample==3?top+1:middle)};
        assert(lv_indev_search_obj(lv_scr_act(),&p)==back);
        pointer(p.x,p.y,true,50);
        if(sample==4)pointer(p.x-14,p.y,true,40); // Small drift outside the initial band stays owned by Back.
        pointer(sample==4?p.x-14:p.x,p.y,false,50);
        if(page<2)assert(button(lv_scr_act(),"I/O") && !button(lv_scr_act(),"Back"));
        else {assert(button(lv_scr_act(),LV_SYMBOL_TINT));click("Back");assert(button(lv_scr_act(),"I/O"));}
        assert(s3::writes==backWrites && s3::volume_writes==backVolumeWrites);
    }
    // Display-only idle must retain latest snapshots without animating or writing.
    const auto idleWrites=s3::writes;
    s3::next.sourceKnown=s3::next.playerKnown=s3::next.connected=true;
    s3::next.input=0;s3::next.playing=true;s3::next.volume=38;
    strcpy(s3::next.title,"Before screen off");s3::pending=true;advance(300);
    assert(has_label(lv_scr_act(),"Before screen off"));
    lv_disp_enable_invalidation(lv_disp_get_default(),false);
    lv_timer_pause(lv_disp_get_default()->refr_timer);
    const unsigned asleepFlushCount=flush_count;
    orbit_ui_set_screen_awake(false);assert(!orbit_ui_record_motion_active());
    auto *idleDisc=image_of_size(lv_scr_act(),170);assert(idleDisc);
    const auto idleShine=record_shine_hash();
    lv_obj_invalidate(idleDisc); // Hidden label/image invalidation must not resume refresh.
    assert(lv_disp_get_default()->refr_timer->paused);
    s3::next.volume=27;strcpy(s3::next.title,"Updated while screen off");
    s3::pending=true;advance(2500);
    assert(!s3::pending && !orbit_ui_record_motion_active() && flush_count==asleepFlushCount);
    assert(record_shine_hash()==idleShine);
    assert(has_label(lv_scr_act(),"Before screen off"));
    lv_disp_enable_invalidation(lv_disp_get_default(),true);
    orbit_ui_set_screen_awake(true);lv_obj_invalidate(lv_scr_act());advance(200);
    assert(has_label(lv_scr_act(),"Updated while screen off"));
    assert(button(lv_scr_act(),LV_SYMBOL_VOLUME_MID"  27"));
    assert(orbit_ui_record_motion_active() && s3::writes==idleWrites);
    // Paused snapshots likewise remain current while the display is dark.
    orbit_ui_set_screen_awake(false);s3::next.playing=false;s3::pending=true;advance(1500);
    assert(!s3::pending);orbit_ui_set_screen_awake(true);advance(200);
    assert(!orbit_ui_record_motion_active() && s3::writes==idleWrites);
    s3::next.input=0;s3::next.sourceKnown=true;s3::next.protocol[0]=0;
    s3::pending=true;advance(300);
    auto *networkLabel=find_named_label(lv_scr_act(),"NETWORK");assert(networkLabel);
    auto *networkImage=lv_obj_get_child(lv_obj_get_parent(networkLabel),0);
    assert(visible(networkImage) && lv_img_get_src(networkImage)==s3::source_icon_image(s3::SourceIcon::Network));
    // A failed read retains the confirmed track/cover as context, never stale control.
    const auto reconnectWrites=s3::writes;
    s3::next.connected=s3::next.sourceKnown=s3::next.playerKnown=true;
    s3::next.volumeKnown=s3::next.muteKnown=s3::next.outputKnown=true;
    s3::next.wifiConnected=true;s3::next.link=s3::LinkState::Ready;
    strcpy(s3::next.protocol,"AirPlay");strcpy(s3::next.title,"Retained during reconnect");
    strcpy(s3::next.artwork,"/retained-cover.jpg");s3::next.artworkRevision=7;
    s3::pending=true;advance(200);s3::nextArtwork=test_artwork(7,"/retained-cover.jpg");
    s3::artworkPending=true;advance(200);
    auto *retainedCover=image_of_size(lv_scr_act(),170);assert(retainedCover);
    const auto *retainedPixels=lv_img_get_src(retainedCover);
    s3::next.connected=false;s3::next.link=s3::LinkState::Waiting;
    s3::next.sourceKnown=s3::next.playerKnown=s3::next.volumeKnown=s3::next.muteKnown=s3::next.outputKnown=false;
    s3::next.volumeLimitKnown=s3::next.positionKnown=false;
    s3::next.canToggle=s3::next.canPlay=s3::next.canPause=s3::next.canNext=s3::next.canPrevious=false;
    s3::pending=true;advance(400);
    assert(has_label(lv_scr_act(),"AIRPLAY") && has_label(lv_scr_act(),"Retained during reconnect"));
    assert(visible(retainedCover) && lv_img_get_src(retainedCover)==retainedPixels);
    assert(footer_is(orbit_status::Icon::Waiting) && !orbit_ui_record_motion_active());
    pointer(400,240,true);pointer(400,240,false);assert(s3::writes==reconnectWrites);
    click("I/O");click("i");assert(has_label(lv_scr_act(),"Reconnecting"));
    assert(has_label(lv_scr_act(),"192.0.2.10") && has_label(lv_scr_act(),"192.0.2.20"));
    click("Back");click("Back");
    s3::next.connected=s3::next.sourceKnown=s3::next.playerKnown=true;
    s3::next.volumeKnown=s3::next.muteKnown=s3::next.outputKnown=true;
    s3::next.link=s3::LinkState::Ready;s3::pending=true;advance(400);
    assert(visible(retainedCover) && lv_img_get_src(retainedCover)==retainedPixels);
    assert(s3::writes==reconnectWrites);
    pointer(240,187,true);pointer(240,187,false);assert(image_of_size(lv_scr_act(),320));
    pointer(240,240,true);s3::next.power=s3::PowerState::Standby;
    s3::pending=true;advance(200);assert(!image_of_size(lv_scr_act(),320));
    pointer(240,240,true);pointer(240,240,false);assert(s3::writes==reconnectWrites);
    assert(button(lv_scr_act(),"Wake"));
    // Four empty pages and actual pointer arbitration, including cover-origin drags.
    s3::next.power=s3::PowerState::Online;s3::next.link=s3::LinkState::Ready;s3::next.connected=true;
    s3::next.powerPending=s3::next.powerTransition=false;s3::next.playing=true;
    s3::pending=true;advance(300);assert(button(lv_scr_act(),"I/O"));
    const auto navigationWrites=s3::writes,navigationVolumeWrites=s3::volume_writes;
    auto assertBlank=[](){assert(!button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Back")&&!footer_visible());
        assert(button(lv_scr_act(),"TIME") || has_label(lv_scr_act(),"Weather") ||
               has_label(lv_scr_act(),"Swiped left") || has_label(lv_scr_act(),"Apple Music"));
        assert_canvas_edge();};
    auto swipePointer=[](int x,int y,int dx,int dy){pointer(x,y,true,70);pointer(x+dx/2,y+dy/2,true,60);pointer(x+dx,y+dy,true,60);pointer(x+dx,y+dy,false,80);advance(360);};
    for(auto delta:{lv_point_t{-110,0},lv_point_t{110,0},lv_point_t{0,-110},lv_point_t{0,110}}){
        pointer(240,187,true,70);pointer(240+delta.x,187+delta.y,true,100);
        assert(button(lv_scr_act(),"I/O")); // Navigation waits for release.
        pointer(240+delta.x,187+delta.y,false,100);advance(240);assertBlank();
        swipePointer(240,240,delta.x,delta.y);assertBlank(); // Further outward ignored.
        if(delta.x)swipePointer(240,240,0,-100);else swipePointer(240,240,-100,0);
        assertBlank(); // Other axes do not move sideways between satellites.
        // Return from an outer-ring coordinate: hidden ring must not claim it.
        if(delta.x<0)swipePointer(17,240,113,0);else swipePointer(240,240,-delta.x,-delta.y);
        assert(button(lv_scr_act(),"I/O") && footer_visible());
        assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    }
    // Diagonal, short drag and expired swipe cancel cover clicks without navigating.
    swipePointer(240,187,70,70);assert(button(lv_scr_act(),"I/O")&&!image_of_size(lv_scr_act(),320));
    swipePointer(240,187,30,0);assert(button(lv_scr_act(),"I/O")&&!image_of_size(lv_scr_act(),320));
    pointer(240,187,true,1300);pointer(130,187,true,60);pointer(130,187,false,80);assert(button(lv_scr_act(),"I/O"));
    // Small drift still counts as a cover tap; dragging its viewer must not close it.
    pointer(240,187,true,60);pointer(250,190,true,60);pointer(250,190,false,80);advance(300);assert(image_of_size(lv_scr_act(),320));
    swipePointer(240,240,-110,0);assert(image_of_size(lv_scr_act(),320));
    pointer(240,240,true);pointer(240,240,false);advance(300);assert(button(lv_scr_act(),"I/O"));
    // Clear Player swipes may begin on buttons, including the lower transport
    // row used for upward gestures. The drag cancels the tap, never sends audio.
    for(auto start:{lv_point_t{150,350},lv_point_t{180,372},lv_point_t{240,320},lv_point_t{240,350},lv_point_t{240,372},lv_point_t{240,405},lv_point_t{240,430},lv_point_t{240,460},lv_point_t{300,372},lv_point_t{330,350}}){
        swipePointer(start.x,start.y,0,-110);assertBlank();
        assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
        swipePointer(240,240,0,110);assert(button(lv_scr_act(),"I/O"));
    }
    // Downward gestures from the header chip follow the same rule.
    for(auto start:{lv_point_t{210,82},lv_point_t{240,82},lv_point_t{270,82}}){
        swipePointer(start.x,start.y,0,110);assertBlank();
        swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"I/O"));
    }
    // Disabled transport remains swipeable; it still cannot issue a command.
    assert(!s3::next.canToggle);swipePointer(240,372,0,-110);assertBlank();
    swipePointer(240,240,0,110);assert(button(lv_scr_act(),"I/O"));
    // Enabled transport also yields to swipes rather than toggling playback.
    s3::next.canToggle=s3::next.canPause=s3::next.canPlay=true;s3::pending=true;advance(300);
    assert(!lv_obj_has_state(button(lv_scr_act(),LV_SYMBOL_PAUSE),LV_STATE_DISABLED));
    swipePointer(240,372,0,-110);assertBlank();assert(s3::writes==navigationWrites);
    swipePointer(240,240,0,110);assert(button(lv_scr_act(),"I/O"));
    // Clear drags on the other buttons likewise cancel their click.
    for(auto start:{lv_point_t{410,240},lv_point_t{70,240},lv_point_t{240,82},lv_point_t{240,372},lv_point_t{240,460}}){
        const int dx=start.x<240?100:-100;
        swipePointer(start.x,start.y,dx,0);assertBlank();
        swipePointer(240,240,-dx,0);assert(button(lv_scr_act(),"I/O"));
        assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    }
    // An interrupted LVGL input session must not turn the still-held drag into a new tap.
    pointer(410,240,true);pointer(350,240,true);
    lv_indev_reset(lv_indev_get_next(nullptr),nullptr);
    pointer(410,240,true);pointer(410,240,false);assert(button(lv_scr_act(),"I/O")&&s3::writes==navigationWrites);
    // Arc-origin drags belong to Volume even over expanded side-button targets.
    // Inward/sideways travel cannot turn them into page navigation; read-only
    // gestures still open Volume without issuing audio commands.
    for(auto start:{lv_point_t{45,240},lv_point_t{435,240},lv_point_t{240,48},lv_point_t{83,397},lv_point_t{397,397}}){
        const int dx=(240-start.x)/2,dy=(240-start.y)/2;
        swipePointer(start.x,start.y,dx,dy);
        assert(button(lv_scr_act(),"Back")&&button(lv_scr_act(),"-"));
        assert(s3::writes==navigationWrites);click("Back");
    }
    // Page gestures can END on the arc: eligibility depends only on the origin.
    swipePointer(150,240,285,0);assertBlank();assert(s3::writes==navigationWrites);
    swipePointer(240,240,-110,0);assert(button(lv_scr_act(),"I/O"));
    // Disabled side-control taps remain inert; their overlapping arc drags are
    // still Volume, never a page gesture or disabled-control activation.
    s3::next.muteKnown=false;s3::pending=true;advance(300);
    pointer(435,240,true);pointer(435,240,false);assert(button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Back"));
    swipePointer(435,240,-110,0);assert(button(lv_scr_act(),"Back")&&button(lv_scr_act(),"-"));assert(s3::writes==navigationWrites);click("Back");
    s3::next.muteKnown=true;s3::pending=true;advance(300);
    click("I/O");swipePointer(240,140,100,0);assert(button(lv_scr_act(),"HDMI"));
    click(LV_SYMBOL_TINT);const auto preferencesWrites=fake_preferences::state().writes;
    swipePointer(240,140,100,0);assert(has_label(lv_scr_act(),"Brightness")&&fake_preferences::state().writes==preferencesWrites);
    click("Back");click("i");swipePointer(240,100,100,0);assert(has_label(lv_scr_act(),"Screen IP"));click("Back");click("Back");
    // Ring retains its existing entry gesture after page navigation.
    // Before drag slop, the contact remains pending: no page or volume action.
    pointer(240,48,true);pointer(250,48,true);
    assert(button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Back"));
    assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    pointer(250,48,false);assert(button(lv_scr_act(),"Back"));click("Back");
    // Slop first exceeded on release still navigates, without clicking Play.
    pointer(240,372,true,70);assert(button(lv_scr_act(),"I/O")&&s3::writes==navigationWrites);
    pointer(240,250,false,80);advance(360);assertBlank();
    swipePointer(240,240,0,110);assert(button(lv_scr_act(),"I/O"));
    pointer(17,240,true);pointer(17,240,false);assert(button(lv_scr_act(),"Back"));click("Back");
    swipePointer(240,187,-110,0);assertBlank();orbit_ui_set_screen_awake(false);advance(300);orbit_ui_set_screen_awake(true);advance(100);assertBlank();
    swipePointer(240,240,110,0);assert(button(lv_scr_act(),"I/O"));
    // Real SNTP display gate, original pixel typography and practical time tools.
    swipePointer(240,187,0,110);assert(has_label(lv_scr_act(),"CONNECT TO WI-FI") && has_label(lv_scr_act(),"--:--"));
    orbit_clock::fixture.status=orbit_clock::Status::Syncing;advance(1200);assert(has_label(lv_scr_act(),"SYNCING TIME"));
    orbit_clock::fixture.status=orbit_clock::Status::Locating;advance(1200);assert(has_label(lv_scr_act(),"FINDING LOCATION")&&!has_label(lv_scr_act(),"PDT"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-syncing.ppm",argv[15]);screenshot(path);}
    orbit_clock::fixture.status=orbit_clock::Status::Ready;strcpy(orbit_clock::fixture.time,"12:34:56");strcpy(orbit_clock::fixture.date,"Sun, Oct 25, 2026");strcpy(orbit_clock::fixture.zone,"PDT");
    orbit_clock::fixture.hour=12;orbit_clock::fixture.minute=34;orbit_clock::fixture.second=56;orbit_clock::fixture.day=25;orbit_clock::fixture.month=10;orbit_clock::fixture.year=2026;orbit_clock::fixture.weekday=0;orbit_clock::fixture.dateKey=20261025;orbit_clock::fixture.epoch=1792956896;
    advance(1200);assert(has_label(lv_scr_act(),"SUN 25 OCT")&&has_label(lv_scr_act(),"12:34")&&has_label(lv_scr_act(),"56")&&has_label(lv_scr_act(),"PDT"));
    assert(!image_of_size(lv_scr_act(),480));assert(lv_obj_get_style_text_font(find_named_label(lv_scr_act(),"12:34"),0)==&orbit_pixel_100);
    assert(has_label(lv_scr_act(),"ALARM OFF")&&button(lv_scr_act(),"SET ALARM"));assert(!button(lv_scr_act(),"Back")&&!footer_visible());
    const auto clockFirstPixels=clock_pixels();if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-ready.ppm",argv[15]);screenshot(path);}
    s3::next.connected=false;s3::pending=true;advance(1200);assert(clock_pixels()==clockFirstPixels);
    orbit_ui_set_screen_awake(false);const auto clockReads=orbit_clock::reads;orbit_clock::fixture.minute=35;orbit_clock::fixture.second=1;advance(2500);assert(orbit_clock::reads==clockReads);orbit_ui_set_screen_awake(true);advance(100);assert(has_label(lv_scr_act(),"12:35"));
    for(unsigned second:{0u,10u,20u,30u,40u,50u}){orbit_clock::fixture.second=second;advance(1200);const auto partial=clock_pixels();lv_obj_invalidate(lv_scr_act());advance(60);assert(clock_pixels()==partial);}
    const auto clockWrites=s3::writes;click("ALRM");assert(has_label(lv_scr_act(),"ALARM")&&button(lv_scr_act(),"07"));click("-");assert(button(lv_scr_act(),"06"));click("OFF");click("SAVE");assert(orbit_time::state().config().enabled&&orbit_time::state().config().hour==6&&has_label(lv_scr_act(),"NEXT ALARM"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-alarm-enabled.ppm",argv[15]);screenshot(path);}
    click("ALRM");if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-alarm-edit.ppm",argv[15]);screenshot(path);}click("TIME");click("ON");assert(!orbit_time::state().config().enabled);
    click("TMR");click("-");assert(orbit_time::state().config().seconds==240);click("00");click("+");assert(orbit_time::state().config().seconds==241);click("START");advance(1100);assert(orbit_time::state().timer()==orbit_time::Timer::Running);click("PAUSE");const auto pausedSeconds=orbit_time::state().remaining();advance(3000);assert(orbit_time::state().remaining()==pausedSeconds&&button(lv_scr_act(),"RESUME"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-timer-paused.ppm",argv[15]);screenshot(path);}click("RESET");assert(orbit_time::state().remaining()==241);orbit_time::preset(1);click("START");
    swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"I/O"));orbit_ui_set_screen_awake(false);advance(1200);assert(orbit_ui_alert_active()&&orbit_ui_keep_screen_on());orbit_ui_set_screen_awake(true);advance(100);assert(button(lv_scr_act(),"STOP")&&has_label(lv_scr_act(),"TIMER DONE"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-timer-alert.ppm",argv[15]);screenshot(path);}swipePointer(240,240,110,0);assert(button(lv_scr_act(),"STOP"));click("STOP");assert(!orbit_ui_alert_active()&&button(lv_scr_act(),"TIME")&&s3::writes==clockWrites);
    // An alert interrupting a held old control cannot consume itself or send audio.
    swipePointer(240,240,0,-110);orbit_time::preset(1);orbit_time::start(lv_tick_get());pointer(70,240,true,200);advance(1000);pointer(70,240,false,100);assert(button(lv_scr_act(),"STOP")&&s3::writes==clockWrites);click("STOP");
    // Revalidate a due cached minute before alarming if timezone/time changed.
    orbit_time::alarm(7,30,false);orbit_clock::hookReads=0;
    orbit_clock::trustedHook=+[]()->orbit_clock::Reading{auto r=orbit_clock::fixture;if(++orbit_clock::hookReads>1){r.hour=(r.hour+1)%24;r.zoneKey=123;}return r;};
    while(!orbit_clock::hookReads)advance(100); // Cache old wall time while disabled, then move zones.
    orbit_time::alarm(orbit_clock::fixture.hour,orbit_clock::fixture.minute,true);advance(200);assert(!orbit_ui_alert_active());orbit_clock::trustedHook=nullptr;orbit_time::alarm(7,30,false);
    // Daily alarm reaches the same persistent, silent visual reminder path.
    orbit_time::alarm(orbit_clock::fixture.hour,orbit_clock::fixture.minute,true);advance(1200);assert(has_label(lv_scr_act(),"ALARM")&&button(lv_scr_act(),"STOP"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-alarm-alert.ppm",argv[15]);screenshot(path);}click("STOP");advance(1200);assert(!orbit_ui_alert_active()&&s3::writes==clockWrites);orbit_time::alarm(7,30,false);
    click("ALRM");click("+");fake_preferences::state().failWrite=true;click("SAVE");assert(button(lv_scr_act(),"SAVE")&&has_label(lv_scr_act(),"NOT SAVED"));fake_preferences::state().failWrite=false;click("SAVE");assert(button(lv_scr_act(),"TIME")&&orbit_time::saved());orbit_time::alarm(7,30,false);
    orbit_time::preset(300);orbit_clock::fixture.hour=0;orbit_clock::fixture.minute=orbit_clock::fixture.second=0;orbit_clock::fixture.day=26;orbit_clock::fixture.weekday=1;orbit_clock::fixture.dateKey=20261026;advance(1200);assert(has_label(lv_scr_act(),"MON 26 OCT")&&has_label(lv_scr_act(),"00:00"));
    orbit_clock::fixture.status=orbit_clock::Status::Offline;advance(1200);assert(has_label(lv_scr_act(),"CONNECT TO WI-FI")&&!has_label(lv_scr_act(),"PDT"));s3::next.connected=true;s3::pending=true;advance(300);swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"I/O"));
    const auto homeClockReads=orbit_clock::reads;advance(1200);assert(orbit_clock::reads==homeClockReads);
    // Weather reads are nonblocking UI snapshots; absent data must never show 0°.
    swipePointer(240,187,0,-110);assert(has_label(lv_scr_act(),"Weather") && has_label(lv_scr_act(),"Finding your area"));
    orbit_weather::fixture.available=true;orbit_weather::fixture.status=orbit_weather::Status::Ready;
    strcpy(orbit_weather::fixture.city,"Los Angeles");auto &weather=orbit_weather::fixture.data;
    weather.temperature=21;weather.high=25;weather.low=16;weather.feels=20;weather.humidity=62;weather.wind=8;weather.rain=20;weather.day=true;weather.code=2;
    advance(1200);assert(has_label(lv_scr_act(),"Near Los Angeles") && has_label(lv_scr_act(),"21°C") && has_label(lv_scr_act(),"H 25°   /   L 16°"));
    assert(has_label(lv_scr_act(),"Partly cloudy") && !footer_visible() && !button(lv_scr_act(),"Back"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/weather-ready.ppm",argv[15]);screenshot(path);}
    assert(has_label(lv_scr_act(),"Feels like 20°C") && has_label(lv_scr_act(),"Wind km/h") && has_label(lv_scr_act(),"Precip today"));
    bool weatherSeen[unsigned(orbit_weather::Icon::Count)]={};unsigned weatherUnique=0;
    for(unsigned code=0;code<100;++code)if(orbit_weather::known_code(code))for(bool day:{false,true}){
        weather.code=code;weather.day=day;advance(1200);
        assert(has_label(lv_scr_act(),orbit_weather::description(code,day)));
        const auto shape=orbit_weather::icon(code,day);const auto *asset=orbit_weather::icon_image(shape);assert(asset);
        const auto *shown=orbit_weather::display_icon(shape,false,day);assert(shown && !memcmp(shown->data,orbit_weather::icon_palette(shape,false,day),64));
        const auto *lightIcon=orbit_weather::display_icon(shape,true,day);assert(lightIcon && !memcmp(lightIcon->data,orbit_weather::icon_palette(shape,true,day),64));
        assert(asset->header.cf==LV_IMG_CF_INDEXED_4BIT && asset->header.w==96 && asset->header.h==96 && asset->data_size==4672);
        // Transparent edge, no clipped glyph at any native image boundary.
        auto pixel=[&](unsigned x,unsigned y){unsigned idx=y*96+x;unsigned v=asset->data[64+idx/2];return (idx&1)?(v&15):(v>>4);};
        for(unsigned edge=0;edge<96;++edge)assert(pixel(edge,0)==0&&pixel(edge,95)==0&&pixel(0,edge)==0&&pixel(95,edge)==0);
        const auto partial=clock_pixels();lv_obj_invalidate(lv_scr_act());advance(60);assert(clock_pixels()==partial);
        if(!weatherSeen[unsigned(shape)]){
            weatherSeen[unsigned(shape)]=true;++weatherUnique;
            if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/weather-condition-%02u-%s.ppm",argv[15],code,day?"day":"night");screenshot(path);}
        }
    }
    assert(weatherUnique==unsigned(orbit_weather::Icon::Count));
    assert(!orbit_weather::icon_image(orbit_weather::Icon::Count));

    weather.code=0;weather.day=false;advance(1200);assert(has_label(lv_scr_act(),"Clear night"));
    orbit_weather::fixture.status=orbit_weather::Status::Saved;orbit_weather::fixture.ageMinutes=24;advance(1200);assert(has_label(lv_scr_act(),"Saved 24m ago"));
    orbit_ui_set_screen_awake(false);const auto weatherReads=orbit_weather::reads;advance(2500);assert(orbit_weather::reads==weatherReads);orbit_ui_set_screen_awake(true);advance(100);assert(has_label(lv_scr_act(),"Clear night"));
    // Round-trip all existing palettes; only local appearance settings change.
    for(unsigned i=0;i<orbit_theme::Count;++i){
        swipePointer(240,240,0,110);click("I/O");click(LV_SYMBOL_TINT);click(orbit_theme::palettes[i].name);click("Back");click("Back");swipePointer(240,187,0,-110);assert_canvas_edge();
        assert(has_label(lv_scr_act(),"Clear night"));
        if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/weather-%s.ppm",argv[15],orbit_theme::palettes[i].id);screenshot(path);}
    }
    orbit_weather::fixture.available=false;orbit_weather::fixture.status=orbit_weather::Status::Offline;advance(1200);assert(has_label(lv_scr_act(),"Connect to Wi-Fi")&&!has_label(lv_scr_act(),"21°C")&&!has_label(lv_scr_act(),"Wind km/h")&&!has_label(lv_scr_act(),"Precip today"));
    orbit_weather::fixture.status=orbit_weather::Status::Unavailable;advance(1200);assert(has_label(lv_scr_act(),"Weather unavailable"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/weather-unavailable.ppm",argv[15]);screenshot(path);}
    swipePointer(240,240,0,110);assert(button(lv_scr_act(),"I/O"));orbit_weather::fixture=orbit_weather::Reading{};
    // Every temporary arrow explicitly identifies the opening gesture and return.
    for(auto delta:{lv_point_t{-110,0},lv_point_t{110,0},lv_point_t{0,-110}}){
        swipePointer(240,187,delta.x,delta.y);
        assert(has_label(lv_scr_act(),delta.x<0?"Swiped left":delta.x>0?"AirPlay":"Weather"));
        if(delta.x)assert(has_label(lv_scr_act(),delta.x<0?"Swipe right to return":"Swipe left to return"));
        if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/swipe-%s.ppm",argv[15],delta.x<0?"left":delta.x>0?"right":"up");screenshot(path);}
        swipePointer(240,240,-delta.x,-delta.y);
    }
    assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    // Left launcher uses one nonblocking request; swiping its button cancels taps.
    swipePointer(240,187,110,0);assert(button(lv_scr_act(),"Play on S3"));
    auto *launchIcon=image_of_size(lv_scr_act(),40);assert(launchIcon && lv_img_get_zoom(launchIcon)==256);
    const auto launchWrites=s3::writes;
    pointer(240,298,true);pointer(240,298,false);assert(orbit_iphone::sends==1 && has_label(lv_scr_act(),"Sending"));
    pointer(240,298,true);pointer(240,298,false);assert(orbit_iphone::sends==1);
    for(auto value:{orbit_iphone::LaunchState::Sent,orbit_iphone::LaunchState::Uncertain,orbit_iphone::LaunchState::Failed,orbit_iphone::LaunchState::Offline,orbit_iphone::LaunchState::Syncing,orbit_iphone::LaunchState::Unconfigured,orbit_iphone::LaunchState::Cooldown}){
        orbit_iphone::fixture=value;advance(200);assert(lv_obj_has_state(button(lv_scr_act(),"Play on S3"),LV_STATE_DISABLED));
    }
    orbit_iphone::fixture=orbit_iphone::LaunchState::Ready;advance(200);
    assert(!lv_obj_has_state(button(lv_scr_act(),"Play on S3"),LV_STATE_DISABLED));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/airplay-launcher.ppm",argv[15]);screenshot(path);}
    swipePointer(240,298,-110,0);assert(button(lv_scr_act(),"I/O"));
    assert(orbit_iphone::sends==1 && s3::writes==launchWrites);
    // Remote standby keeps local pages; an interrupted contact cannot become Wake.
    swipePointer(240,187,0,-110);assertBlank();pointer(240,240,true);
    s3::next.power=s3::PowerState::Standby;s3::pending=true;advance(200);assertBlank();
    pointer(350,240,true);pointer(350,240,false);assertBlank();assert(s3::writes==navigationWrites);
    swipePointer(240,240,0,110);assert(button(lv_scr_act(),"Wake"));
    // Standby unlocks all four directions from the central Wake target. A swipe
    // never wakes S3 or changes volume, and normal standby reports retain pages.
    for(auto delta:{lv_point_t{-110,0},lv_point_t{110,0},lv_point_t{0,-110},lv_point_t{0,110}}){
        swipePointer(240,240,delta.x,delta.y);assertBlank();
        s3::pending=true;advance(200);assertBlank();
        if(delta.x>0){
            assert(!lv_obj_has_state(button(lv_scr_act(),"Play on S3"),LV_STATE_DISABLED));
        }
        swipePointer(240,240,-delta.x,-delta.y);assert(button(lv_scr_act(),"Wake"));
        assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    }
    // Hidden standby arc is ordinary navigation space, not a Volume gesture.
    swipePointer(240,48,0,110);assert(button(lv_scr_act(),"TIME"));
    swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"Wake"));
    s3::next.power=s3::PowerState::Online;s3::pending=true;advance(300);assert(button(lv_scr_act(),"I/O"));
    assert(s3::writes==navigationWrites && s3::volume_writes==navigationVolumeWrites);
    // Whole-page exit/entry: ring, source/header, footer and transport must all
    // have the same actual displacement, not merely animated center content.
    orbit_clock::fixture.status=orbit_clock::Status::Ready;
    const auto slideWrites=s3::writes,slideVolumeWrites=s3::volume_writes;
    const auto animationsBefore=lv_anim_count_running();
    auto frameHash=[](){uint32_t h=2166136261u;for(const auto &pixel : ::frame){h^=pixel.full;h*=16777619u;}return h;};
    auto dumpFrame=[](const char *path){
        FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n480 480\n255\n");
        for(const auto color : ::frame){lv_color32_t rgb;rgb.full=lv_color_to32(color);const unsigned char bytes[]={rgb.ch.red,rgb.ch.green,rgb.ch.blue};fwrite(bytes,1,3,file);}fclose(file);
    };
    auto startSlide=[](int dx,int dy){pointer(240,187,true,60);pointer(240+dx,187+dy,true,70);pointer(240+dx,187+dy,false,40);advance(20);};
    auto *homePage=lv_obj_get_parent(button(lv_scr_act(),"I/O")),*surface=lv_obj_get_parent(homePage);
    auto *volumeArc=lv_obj_get_child(surface,0);assert(lv_obj_check_type(volumeArc,&lv_arc_class));
    lv_obj_t *wholeGroup[]={volumeArc,button(lv_scr_act(),LV_SYMBOL_VOLUME_MID"  "),find_named_label(lv_scr_act(),"AIRPLAY"),footer_target(lv_scr_act()),footer_image(lv_scr_act()),button(lv_scr_act(),"I/O"),button(lv_scr_act(),LV_SYMBOL_PAUSE),image_of_size(lv_scr_act(),170)};
    lv_area_t bases[8];for(unsigned i=0;i<8;++i){assert(wholeGroup[i]);lv_obj_get_coords(wholeGroup[i],&bases[i]);}
    auto assertGroup=[&](bool horizontal,int offset){
        lv_obj_update_layout(surface);
        for(unsigned i=0;i<8;++i){lv_area_t actual;lv_obj_get_coords(wholeGroup[i],&actual);
            assert(actual.x1-bases[i].x1==(horizontal?offset:0));
            assert(actual.y1-bases[i].y1==(horizontal?0:offset));}
        assert(lv_obj_get_style_translate_x(surface,0)==0&&lv_obj_get_style_translate_y(surface,0)==0);
    };
    for(auto delta:{lv_point_t{-110,0},lv_point_t{110,0},lv_point_t{0,-110},lv_point_t{0,110}}){
        const bool horizontal=delta.x!=0;
        startSlide(delta.x,delta.y);assert(button(lv_scr_act(),"I/O")); // Old page really exits first.
        const int outgoing=horizontal?lv_obj_get_style_translate_x(homePage,0):lv_obj_get_style_translate_y(homePage,0);
        assert(outgoing!=0&&abs(outgoing)<=48);assert((outgoing<0)==(delta.x<0||delta.y<0));
        assertGroup(horizontal,outgoing);
        if(delta.y>0&&argc>15){for(int i=0;i<5;++i){char path[512];snprintf(path,sizeof(path),"%s/clock-motion-%02d.ppm",argv[15],i);dumpFrame(path);advance(20);}}
        else advance(100);
        auto *caption=find_named_label(lv_scr_act(),delta.x<0?"Swiped left":delta.x>0?"AirPlay":delta.y<0?"Weather":"ALARM OFF");assert(caption);
        auto *page=lv_obj_get_parent(caption);
        const int before=horizontal?lv_obj_get_style_translate_x(page,0):lv_obj_get_style_translate_y(page,0);
        assert(before!=0&&abs(before)<=64);assert((before>0)==(delta.x<0||delta.y<0));
        assert((horizontal?lv_obj_get_style_translate_y(page,0):lv_obj_get_style_translate_x(page,0))==0);
        const auto pixels=frameHash();
        if(delta.y>0&&argc>15){for(int i=5;i<14;++i){char path[512];snprintf(path,sizeof(path),"%s/clock-motion-%02d.ppm",argv[15],i);dumpFrame(path);advance(20);}}
        else advance(50);
        const int after=horizontal?lv_obj_get_style_translate_x(page,0):lv_obj_get_style_translate_y(page,0);
        assert(abs(after)<abs(before)&&frameHash()!=pixels);advance(280);
        if(delta.y>0&&argc>15){char path[512];snprintf(path,sizeof(path),"%s/clock-motion-14.ppm",argv[15]);dumpFrame(path);}
        for(uint32_t i=0;i<lv_obj_get_child_cnt(surface);++i){auto *child=lv_obj_get_child(surface,i);assert(lv_obj_get_style_translate_x(child,0)==0&&lv_obj_get_style_translate_y(child,0)==0);}
        assert(lv_anim_count_running()==animationsBefore);assert_canvas_edge();
        // Return has a real old-page exit and uniform Home entry, including chrome.
        startSlide(-delta.x,-delta.y);advance(120);assert(button(lv_scr_act(),"I/O"));
        const int incoming=horizontal?lv_obj_get_style_translate_x(homePage,0):lv_obj_get_style_translate_y(homePage,0);
        assert(incoming!=0&&abs(incoming)<=64);assertGroup(horizontal,incoming);advance(280);assertGroup(horizontal,0);
        auto *entryCover=image_of_size(lv_scr_act(),170);assert(entryCover&&lv_img_get_zoom(entryCover)==256);
    }
    // Contact arriving mid-entry remains suppressed across completion and an
    // LVGL input reset; only a fresh contact can activate a control.
    swipePointer(240,187,-110,0);startSlide(110,0);advance(120);assert(button(lv_scr_act(),"I/O"));
    pointer(70,240,true,40);advance(260);lv_indev_reset(lv_indev_get_next(nullptr),nullptr);
    pointer(70,240,true,40);pointer(70,240,false,60);assert(button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Back"));
    pointer(70,240,true);pointer(70,240,false);assert(button(lv_scr_act(),"HDMI"));click("Back");
    // The same guard prevents an animated-entry contact becoming ring volume.
    swipePointer(240,187,-110,0);startSlide(110,0);advance(120);
    pointer(17,240,true,40);advance(260);pointer(48,240,true);pointer(48,240,false);
    assert(button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Back"));
    // Idle cancellation snaps to exact destination geometry; it cannot repaint
    // later or restart a transition on wake.
    startSlide(0,110);assert(button(lv_scr_act(),"I/O")); // Idle interrupts outgoing phase.
    orbit_ui_set_screen_awake(false);auto *clockLocation=find_named_label(lv_scr_act(),"ALARM OFF");assert(clockLocation);
    assert(lv_obj_get_style_translate_y(lv_obj_get_parent(clockLocation),0)==0);
    assert(lv_anim_count_running()==animationsBefore);advance(500);orbit_ui_set_screen_awake(true);advance(100);
    assert(button(lv_scr_act(),"TIME")&&lv_obj_get_style_translate_y(clockLocation,0)==0);
    swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"I/O"));
    // External standby during Home entry keeps the destination; held contact cannot Wake.
    swipePointer(240,187,0,110);startSlide(0,-110);pointer(240,240,true,40);
    s3::next.power=s3::PowerState::Standby;s3::pending=true;advance(200);assert(button(lv_scr_act(),"Wake"));
    pointer(240,240,true,60);pointer(240,240,false,80);assert(s3::writes==slideWrites);
    s3::next.power=s3::PowerState::Online;s3::pending=true;advance(300);assert(button(lv_scr_act(),"I/O"));
    startSlide(0,110);s3::next.power=s3::PowerState::Standby;s3::pending=true;advance(400);
    assert(button(lv_scr_act(),"TIME") && !button(lv_scr_act(),"Wake"));
    swipePointer(240,240,0,-110);assert(button(lv_scr_act(),"Wake"));
    s3::next.power=s3::PowerState::Online;s3::pending=true;advance(300);
    for(int i=0;i<20;++i){swipePointer(240,187,-110,0);swipePointer(240,240,110,0);assert(lv_anim_count_running()==animationsBefore);}
    assert(s3::writes==slideWrites&&s3::volume_writes==slideVolumeWrites);assert_canvas_edge();
    // Integrated wake-before-phone ordering: exactly one Wake, then only a new
    // confirmed Online report can send. Leaving the page/display-off is safe.
    s3::accept=true;orbit_iphone::fixture=orbit_iphone::LaunchState::Ready;
    auto openStandbyLaunch=[&](){
        s3::next.connected=true;s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Standby;
        s3::next.powerPending=s3::next.powerTransition=false;s3::pending=true;advance(400);
        if(!button(lv_scr_act(),"Play on S3"))swipePointer(240,240,110,0);
        assert(button(lv_scr_act(),"Play on S3"));
    };
    openStandbyLaunch();auto wakeRequests=s3::writes;auto phoneRequests=orbit_iphone::sends;
    click("Play on S3");assert(s3::writes==wakeRequests+1&&s3::last_action==s3::Action::Wake);
    assert(orbit_iphone::sends==phoneRequests&&has_label(lv_scr_act(),"Waking S3"));
    pointer(240,298,true);pointer(240,298,false);assert(s3::writes==wakeRequests+1);
    s3::next.connected=false;s3::next.link=s3::LinkState::Waiting;s3::pending=true;advance(200);
    assert(orbit_iphone::sends==phoneRequests&&has_label(lv_scr_act(),"Waking S3"));
    s3::next.connected=true;s3::next.link=s3::LinkState::Ready;
    s3::next.power=s3::PowerState::Online;s3::next.powerTransition=true;s3::next.powerPending=true;s3::pending=true;advance(300);
    assert(orbit_iphone::sends==phoneRequests);
    swipePointer(240,298,-110,0);assert(button(lv_scr_act(),"I/O"));orbit_ui_set_screen_awake(false);
    s3::next.powerTransition=s3::next.powerPending=false;s3::pending=true;advance(300);
    assert(orbit_iphone::sends==phoneRequests+1&&s3::writes==wakeRequests+1);
    s3::pending=true;advance(300);assert(orbit_iphone::sends==phoneRequests+1);
    orbit_ui_set_screen_awake(true);advance(100);
    // Disconnection cancels instead of resuming a stale launch after reconnect.
    orbit_iphone::fixture=orbit_iphone::LaunchState::Ready;openStandbyLaunch();phoneRequests=orbit_iphone::sends;
    click("Play on S3");s3::next.connected=false;s3::next.link=s3::LinkState::Offline;s3::pending=true;advance(200);
    s3::next.connected=true;s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Online;s3::pending=true;advance(200);
    assert(orbit_iphone::sends==phoneRequests&&has_label(lv_scr_act(),"Could not wake S3"));
    // No fresh confirmation, including a stale cached Online, cannot trigger.
    openStandbyLaunch();click("Play on S3");advance(10200);assert(orbit_iphone::sends==phoneRequests);
    s3::next.power=s3::PowerState::Online;s3::pending=true;advance(200);assert(orbit_iphone::sends==phoneRequests);
    // Rejected Wake command and explicit device error likewise stop the flow.
    openStandbyLaunch();s3::accept=false;click("Play on S3");assert(orbit_iphone::sends==phoneRequests&&has_label(lv_scr_act(),"Could not wake S3"));s3::accept=true;
    advance(4200);s3::pending=true;advance(100);click("Play on S3");s3::next.link=s3::LinkState::Error;s3::pending=true;advance(200);
    assert(orbit_iphone::sends==phoneRequests);
    s3::next.link=s3::LinkState::Ready;s3::next.power=s3::PowerState::Online;s3::pending=true;advance(200);assert(orbit_iphone::sends==phoneRequests);
    // Lyrics use the same interpolated timeline, without creating audio commands.
    if(button(lv_scr_act(),"Play on S3"))swipePointer(240,240,-110,0);
    assert(button(lv_scr_act(),"I/O"));
    s3::next.connected=s3::next.sourceKnown=s3::next.playerKnown=true;s3::next.power=s3::PowerState::Online;s3::next.link=s3::LinkState::Ready;
    s3::next.powerPending=s3::next.powerTransition=false;s3::next.input=0;s3::next.duration=180;s3::next.playing=false;
    strcpy(s3::next.title,"Synthetic lyric track");strcpy(s3::next.artist,"Test artist");s3::next.album[0]=0;
    strcpy(s3::next.artwork,"/lyric-cover.jpg");s3::next.artworkRevision=77;
    s3::next.positionKnown=true;s3::next.positionMs=1000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;
    ++orbit_lyrics::generation;s3::pending=true;advance(200);assert(!button(lv_scr_act(),"Lyrics"));
    auto *lyricDoc=static_cast<orbit_lyrics::Document*>(calloc(1,sizeof(orbit_lyrics::Document)));assert(lyricDoc);
    orbit_lyrics::Track lyricTrack;strcpy(lyricTrack.title,s3::next.title);strcpy(lyricTrack.artist,s3::next.artist);lyricTrack.duration=180;
    const char *lrc="[00:00]First synthetic line\n[00:10]中文歌词 / 日本語の歌詞\n[00:20]A longer synthetic line that wraps naturally across the round display\n[00:30]Final synthetic line\n[00:40]";
    assert(orbit_lyrics::parse_lrc(lrc,strlen(lrc),lyricTrack,*lyricDoc));
    orbit_lyrics::next=lyricDoc;orbit_lyrics::pending=true;advance(200);assert(button(lv_scr_act(),"Lyrics"));
    assert(lv_obj_get_style_bg_opa(button(lv_scr_act(),"Lyrics"),0)==LV_OPA_TRANSP && lv_obj_get_style_border_width(button(lv_scr_act(),"Lyrics"),0)==0);
    assert(image_of_size(button(lv_scr_act(),"Lyrics"),28));
    assert_touch_targets();const auto lyricWrites=s3::writes,lyricVolumeWrites=s3::volume_writes;
    // Works over the record fallback, then over a real cover; the badge is topmost.
    lv_point_t lyricPoint={342,112};assert(lv_indev_search_obj(lv_scr_act(),&lyricPoint)==button(lv_scr_act(),"Lyrics"));
    s3::nextArtwork=test_artwork(77,"/lyric-cover.jpg");s3::artworkPending=true;advance(200);assert(image_of_size(lv_scr_act(),170));
    assert(lv_indev_search_obj(lv_scr_act(),&lyricPoint)==button(lv_scr_act(),"Lyrics"));
    auto *lyricMark=image_of_size(button(lv_scr_act(),"Lyrics"),28);assert(lyricMark);
    const auto *lyricMask=static_cast<const lv_img_dsc_t*>(lv_img_get_src(lyricMark));int visualX=28,visualY=28;
    for(int y=0;y<28;++y)for(int x=0;x<28;++x)if(lyricMask->data[y*28+x]>48){if(x<visualX)visualX=x;if(y<visualY)visualY=y;}
    lv_area_t markArea,coverArea;lv_obj_get_coords(lyricMark,&markArea);lv_obj_get_coords(image_of_size(lv_scr_act(),170),&coverArea);
    assert(abs(markArea.y1+visualY-coverArea.y1-3)<=1 && markArea.x1+visualX>coverArea.x2 && markArea.x1+visualX-coverArea.x2<=10);
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/lyrics-entry.ppm",argv[15]);screenshot(path);}
    pointer(342,112,true);pointer(342,112,false);assert(!has_label(lv_scr_act(),"Lyrics")&&has_label(lv_scr_act(),"First synthetic line")&&!button(lv_scr_act(),"I/O")&&!footer_visible());
    auto *firstLyric=find_named_label(lv_scr_act(),"First synthetic line");assert(firstLyric);const auto activeLyricColor=lv_obj_get_style_text_color(firstLyric,0).full;
    assert(orbit_ui_keep_screen_on() && lv_obj_get_style_text_opa(firstLyric,0)==255);
    assert(lv_obj_get_style_text_opa(find_named_label(lv_scr_act(),"中文歌词 / 日本語の歌詞"),0)==160);
    s3::next.playing=true;s3::next.positionMs=8500;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(2300);
    auto *secondLyric=find_named_label(lv_scr_act(),"中文歌词 / 日本語の歌詞");assert(secondLyric&&lv_obj_get_style_text_color(secondLyric,0).full==activeLyricColor&&lv_obj_get_style_text_opa(secondLyric,0)==255);
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/lyrics-synced.ppm",argv[15]);screenshot(path);}
    s3::next.playing=false;s3::next.positionMs=12000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(5000);
    assert(lv_obj_get_style_text_color(find_named_label(lv_scr_act(),"中文歌词 / 日本語の歌詞"),0).full==activeLyricColor);
    s3::next.positionMs=22000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(120);
    auto *movingLine=find_named_label(lv_scr_act(),"A longer synthetic line that wraps naturally across the round display");assert(movingLine);
    assert(lv_obj_get_style_translate_y(movingLine,0)>0);const int movingY=lv_obj_get_y(movingLine)+lv_obj_get_style_translate_y(movingLine,0);
    advance(400);assert(lv_obj_get_style_translate_y(movingLine,0)==0&&lv_obj_get_y(movingLine)<movingY&&lv_obj_get_style_text_opa(movingLine,0)==255);
    s3::next.positionMs=1000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(450);
    assert(lv_obj_get_style_text_color(find_named_label(lv_scr_act(),"First synthetic line"),0).full==activeLyricColor);
    const auto lyricPixels=clock_pixels();lv_obj_invalidate(lv_scr_act());advance(60);assert(clock_pixels()==lyricPixels);
    orbit_ui_set_screen_awake(false);s3::next.positionMs=22000;s3::next.positionSampleMs=lv_tick_get();++s3::next.positionSerial;s3::pending=true;advance(2000);orbit_ui_set_screen_awake(true);advance(200);
    assert(has_label(lv_scr_act(),"A longer synthetic line that wraps naturally across the round display"));
    pointer(240,240,true);pointer(240,240,false);assert(button(lv_scr_act(),"I/O")&&button(lv_scr_act(),"Lyrics"));
    assert(!orbit_ui_keep_screen_on());
    // A clear swipe starting on the badge stays a page swipe, not a badge tap.
    swipePointer(342,112,-110,0);assert(has_label(lv_scr_act(),"Swiped left"));swipePointer(240,240,110,0);assert(button(lv_scr_act(),"Lyrics"));
    pointer(342,112,true);pointer(342,112,false);assert(!has_label(lv_scr_act(),"Lyrics")&&!button(lv_scr_act(),"I/O"));
    ++orbit_lyrics::generation;strcpy(s3::next.title,"A different track");s3::pending=true;advance(200);assert(button(lv_scr_act(),"I/O")&&!button(lv_scr_act(),"Lyrics"));
    assert(s3::writes==lyricWrites&&s3::volume_writes==lyricVolumeWrites);
    // Radial music choices + real continuous LVGL reading, no added audio writes.
    const auto introWrites=s3::writes,introVolumeWrites=s3::volume_writes;
    strcpy(s3::next.title,"A Place Between");strcpy(s3::next.artist,"Northline");strcpy(s3::next.album,"Still / Moving");
    s3::next.input=0;s3::next.playerKnown=s3::next.sourceKnown=s3::next.connected=true;s3::pending=true;advance(300);
    assert(button(lv_scr_act(),"Music information"));assert_touch_targets();
    swipePointer(240,304,-110,0);assert(has_label(lv_scr_act(),"Swiped left"));swipePointer(240,240,110,0);
    pointer(240,300,true);assert(!button(lv_scr_act(),"Artist"));pointer(240,300,false);
    assert(button(lv_scr_act(),"Artist")&&button(lv_scr_act(),"Release")&&button(lv_scr_act(),"Song")&&!footer_visible());
    assert(!has_label(lv_scr_act(),"Explore")&&!has_label(lv_scr_act(),"People & influences"));
    assert(has_label(lv_scr_act(),"A Place Between")&&has_label(lv_scr_act(),"Northline"));assert_touch_targets();
    lv_area_t artistChoice,albumChoice,songChoice;lv_obj_get_coords(button(lv_scr_act(),"Artist"),&artistChoice);lv_obj_get_coords(button(lv_scr_act(),"Release"),&albumChoice);lv_obj_get_coords(button(lv_scr_act(),"Song"),&songChoice);
    assert(lv_obj_get_style_text_font(lv_obj_get_child(button(lv_scr_act(),"Artist"),0),0)==&lv_font_montserrat_16);
    assert(artistChoice.y1<albumChoice.y1 && albumChoice.y1==songChoice.y1 && albumChoice.x2<artistChoice.x1 && songChoice.x1>artistChoice.x2);
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-menu.ppm",argv[15]);screenshot(path);}
    click("Artist");assert(has_label(lv_scr_act(),"Artist")&&!has_label(lv_scr_act(),"Artist / Preview")&&!has_label(lv_scr_act(),"1 / 2"));
    assert(!orbit_ui_keep_screen_on());assert_touch_targets();
    assert(has_label(lv_scr_act(),"Loading"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-loading.ppm",argv[15]);screenshot(path);}
    advance(1700);assert(has_label(lv_scr_act(),"Local sample")&&!has_label(lv_scr_act(),"Loading"));
    auto *readerViewport=reading_view(lv_scr_act());assert(readerViewport);lv_obj_update_layout(readerViewport);assert(lv_obj_get_scroll_bottom(readerViewport)>0);
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-reader.ppm",argv[15]);screenshot(path);}
    const auto sourceStarts=orbit_ui_intro_mock_starts();
    assert(!button(lv_scr_act(),"Sources"));assert(has_label(lv_scr_act(),"Source"));
    auto *sourceHeading=find_named_label(lv_scr_act(),"Source");auto *firstRef=button(lv_scr_act(),"[1]");assert(sourceHeading&&firstRef&&lv_obj_get_parent(firstRef)==readerViewport);
    assert(lv_obj_get_child_cnt(firstRef)==2 && !strcmp(lv_label_get_text(lv_obj_get_child(firstRef,1)),"Local sample (synthetic)"));
    // The references follow both full paragraphs; they do not occupy the header.
    assert(lv_obj_get_y(sourceHeading)>lv_obj_get_y(lv_obj_get_child(readerViewport,1))+lv_obj_get_height(lv_obj_get_child(readerViewport,1)));
    assert(lv_obj_get_y(firstRef)>lv_obj_get_y(sourceHeading));
    lv_obj_scroll_to_view(firstRef,LV_ANIM_OFF);advance();const auto referenceScroll=lv_obj_get_scroll_y(readerViewport);assert(referenceScroll>0);
    assert_touch_targets();
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-reference-list.ppm",argv[15]);screenshot(path);}
    click("[1]");assert(has_label(lv_scr_act(),"example.org")&&has_label(lv_scr_act(),"Local sample (synthetic)"));assert_touch_targets();
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-sources.ppm",argv[15]);screenshot(path);}
    click("Back");assert(reading_view(lv_scr_act())==readerViewport && orbit_ui_intro_mock_starts()==sourceStarts);assert(lv_obj_get_scroll_y(readerViewport)==referenceScroll);
    for(unsigned cycle=0;cycle<24;++cycle){click("[1]");click("Back");assert(orbit_ui_intro_mock_starts()==sourceStarts);}
    // Named end-of-reading references open the exact retained source.
    auto *ref=button(lv_scr_act(),"[1]");assert(ref);lv_event_send(ref,LV_EVENT_CLICKED,nullptr);advance();assert(has_label(lv_scr_act(),"example.org"));
    swipePointer(240,230,110,0);assert(reading_view(lv_scr_act())==readerViewport && orbit_ui_intro_mock_starts()==sourceStarts);
    // Maximum four-source UI, long Unicode title and383-byte URL use bounded QR memory.
    auto *sourceValue=const_cast<orbit_intro_ai::Introduction*>(orbit_intro_runtime::client()->result());assert(sourceValue);
    auto *sourceBackup=new orbit_intro_ai::Introduction(*sourceValue);sourceValue->sourceCount=4;sourceValue->paragraphSources[0]=15;sourceValue->paragraphSources[1]=8;
    for(unsigned i=1;i<4;++i){snprintf(sourceValue->sources[i].title,sizeof sourceValue->sources[i].title,"Résumé / 日本語 / A long reference title for the selected recording %u",i+1);snprintf(sourceValue->sources[i].url,sizeof sourceValue->sources[i].url,"https://example.org/reference/%u",i+1);}
    auto &longRef=sourceValue->sources[3];size_t longSize=strlen(longRef.url);memset(longRef.url+longSize,'x',383-longSize);longRef.url[383]=0;
    orbit_ui_set_screen_awake(false);orbit_ui_set_screen_awake(true);advance();
    auto *lastRef=button(lv_scr_act(),"[4]");assert(lastRef&&lv_obj_get_child_cnt(lastRef)==2);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(lastRef,1)),sourceValue->sources[3].title));
    assert(lv_obj_get_y(lastRef)>lv_obj_get_y(button(lv_scr_act(),"[3]")));
    lv_obj_scroll_to_view(lastRef,LV_ANIM_OFF);advance();assert_touch_targets();
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-reference-list-long.ppm",argv[15]);screenshot(path);}
    click("[1]");assert(button(lv_scr_act(),"[4]"));click("[4]");assert_touch_targets();
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-sources-long.ppm",argv[15]);screenshot(path);}
    click("Back");*sourceValue=*sourceBackup;delete sourceBackup;orbit_ui_set_screen_awake(false);orbit_ui_set_screen_awake(true);
    puts("PASS: end-of-reading Source heading/named wrapped list, four retained sources, long Unicode labels/383-byte URL,24 QR open-close cycles, captured cache/scroll and theme screenshots; no network");
    qr_track=true;
    for(unsigned mask:{1u,2u,3u}){qr_fault_mask=mask;qr_fault_count=0;click("[1]");assert(qr_fault_count==2);qr_fault_mask=0;
        assert(has_label(lv_scr_act(),"https://example.org/local-introduction") && qr_live_count==0);assert_touch_targets();click("Back");assert(qr_live_count==0);}
    for(unsigned n=0;n<24;++n){click("[1]");assert(qr_live_count==1);click("Back");assert(qr_live_count==0);}
    assert(qr_live_peak==2);qr_track=false;
    puts("PASS: QR data/scratch/both allocation failures fall back to static URL,24 normal cycles retain822 bytes and release all QR allocations, peak1644 bytes; actual LVGL allocator interception");
    // Position must change WHILE the finger is down, not just after release.
    lv_obj_scroll_to_y(readerViewport,0,LV_ANIM_OFF);advance();
    pointer(240,330,true);pointer(240,285,true);const auto dragged=lv_obj_get_scroll_y(readerViewport);assert(dragged>0);
    pointer(240,240,true);assert(lv_obj_get_scroll_y(readerViewport)>dragged);pointer(240,240,false);advance(400);
    assert(reading_view(lv_scr_act())==readerViewport && !button(lv_scr_act(),"I/O"));
    const auto beforeDown=lv_obj_get_scroll_y(readerViewport);pointer(240,205,true);pointer(240,270,true);assert(lv_obj_get_scroll_y(readerViewport)<beforeDown);pointer(240,280,false);advance(500);
    // A reading drag may end over Back without turning into a button tap.
    pointer(240,280,true);pointer(240,350,true);pointer(240,430,true);pointer(240,430,false);advance(500);assert(reading_view(lv_scr_act())==readerViewport);
    click("Back");click("Release");advance(1700);click("Back");click("Song");advance(1700);click("Back");
    const auto tourStarts=orbit_ui_intro_mock_starts();
    for(const char *topic:{"Artist","Release","Song"}){click(topic);assert(has_label(lv_scr_act(),"Local sample | Saved")&&orbit_ui_intro_mock_starts()==tourStarts);click("Back");}
    click("Back");click("Music information");click("Artist");assert(has_label(lv_scr_act(),"Local sample | Saved")&&orbit_ui_intro_mock_starts()==tourStarts);
    strcpy(s3::next.title,"A new track");strcpy(s3::next.artist,"A new artist");s3::next.album[0]=0;s3::pending=true;advance(200);
    assert(has_label(lv_scr_act(),"Northline")&&!has_label(lv_scr_act(),"A new artist"));
    orbit_ui_set_screen_awake(false);advance(100);orbit_ui_set_screen_awake(true);assert(has_label(lv_scr_act(),"Northline"));
    swipePointer(240,230,110,0);assert(button(lv_scr_act(),"Artist"));
    assert(has_label(lv_scr_act(),"A new artist"));click("Artist");assert(has_label(lv_scr_act(),"Loading"));advance(1700);click("Back");
    click("Release");advance(1700);assert(has_label(lv_scr_act(),"Release not identified"));click("Back");
    click("Song");advance(1700);assert(has_label(lv_scr_act(),"A new track"));click("Back");click("Back");
    assert(has_label(lv_scr_act(),"A new track")&&button(lv_scr_act(),"I/O"));
    pointer(240,326,true);pointer(240,326,false);assert(!lv_obj_has_state(button(lv_scr_act(),"Release"),LV_STATE_DISABLED));click("Release");
    advance(1700);assert(has_label(lv_scr_act(),"Release")&&has_label(lv_scr_act(),"A new track")&&has_label(lv_scr_act(),"Release not identified")&&!has_label(lv_scr_act(),"Release / Preview"));
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-release-unknown.ppm",argv[15]);screenshot(path);}
    strcpy(s3::next.album,"Another release");s3::pending=true;advance(200);assert(has_label(lv_scr_act(),"Release not identified")&&!has_label(lv_scr_act(),"Another release"));click("Back");click("Back");
    s3::next.artist[0]=0;s3::next.album[0]=0;s3::pending=true;advance(200);click("Music information");assert(lv_obj_has_state(button(lv_scr_act(),"Release"),LV_STATE_DISABLED));click("Back");
    s3::next.title[0]=0;strcpy(s3::next.artist,"Artist only");s3::pending=true;advance(200);click("Music information");assert(lv_obj_has_state(button(lv_scr_act(),"Release"),LV_STATE_DISABLED));click("Back");

    strcpy(s3::next.title,"Canción después de medianoche / 幸せのものさし / Anniversary Recording");strcpy(s3::next.artist,"Un très long nom d'artiste avec des accents / 日本語 / Northline");
    strcpy(s3::next.album,"Álbum");s3::pending=true;advance(200);click("Music information");
    auto *menuLongAuthor=find_named_label(lv_scr_act(),s3::next.artist);assert(menuLongAuthor&&lv_label_get_long_mode(menuLongAuthor)==LV_LABEL_LONG_SCROLL_CIRCULAR);advance(900);
    const auto menuOffset=reinterpret_cast<lv_label_t*>(menuLongAuthor)->offset.x;assert(menuOffset<0);auto *menuLongTitle=find_named_label(lv_scr_act(),s3::next.title);assert(menuLongTitle&&reinterpret_cast<lv_label_t*>(menuLongTitle)->offset.x<0);s3::pending=true;advance(500);assert(reinterpret_cast<lv_label_t*>(menuLongAuthor)->offset.x<menuOffset);
    if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-long-metadata.ppm",argv[15]);screenshot(path);}
    orbit_ui_set_screen_awake(false);assert(lv_label_get_long_mode(menuLongAuthor)==LV_LABEL_LONG_DOT&&lv_label_get_long_mode(menuLongTitle)==LV_LABEL_LONG_DOT);orbit_ui_set_screen_awake(true);assert(lv_label_get_long_mode(menuLongAuthor)==LV_LABEL_LONG_SCROLL_CIRCULAR);
    bool wrapped[2]={},tail[2]={};int previous[2]={0,0};lv_obj_t *longLabels[]={menuLongTitle,menuLongAuthor};
    const auto introSavedPlaying=s3::next.playing;s3::next.playing=false;
    for(unsigned frame=0;frame<360;++frame){if(frame%5==0)s3::pending=true;advance(100);
        for(unsigned i=0;i<2;++i){auto *o=longLabels[i];const int offset=reinterpret_cast<lv_label_t*>(o)->offset.x;
            if(offset>previous[i]+10)wrapped[i]=true;previous[i]=offset;
            const int extent=lv_txt_get_width(lv_label_get_text(o),strlen(lv_label_get_text(o)),lv_obj_get_style_text_font(o,0),0,LV_TEXT_FLAG_NONE)-lv_obj_get_width(o);
            if(offset<=-extent)tail[i]=true;
        }
    }
    assert(wrapped[0]&&wrapped[1]&&tail[0]&&tail[1]);s3::next.playing=introSavedPlaying;
    click("Artist");assert(lv_label_get_long_mode(menuLongAuthor)==LV_LABEL_LONG_DOT&&lv_label_get_long_mode(menuLongTitle)==LV_LABEL_LONG_DOT);
    auto *introLong=find_named_label(lv_scr_act(),s3::next.artist);assert(introLong&&lv_label_get_long_mode(introLong)==LV_LABEL_LONG_SCROLL_CIRCULAR);
    orbit_ui_set_screen_awake(false);assert(lv_label_get_long_mode(introLong)==LV_LABEL_LONG_DOT);orbit_ui_set_screen_awake(true);assert(lv_label_get_long_mode(introLong)==LV_LABEL_LONG_SCROLL_CIRCULAR);
    advance(1700);
    for(int i=0;i<8;++i){click("Back");click("Back");click("I/O");click(LV_SYMBOL_TINT);click(orbit_theme::palettes[i].name);click("Back");click("Back");click("Music information");
        if(argc>15 && i==5){char path[512];snprintf(path,sizeof(path),"%s/music-intro-menu-cream.ppm",argv[15]);screenshot(path);}
        click("Song");advance(1700);assert(reading_view(lv_scr_act()));assert_touch_targets();
        if(argc>15 && i==5){char path[512];snprintf(path,sizeof(path),"%s/music-intro-reader-cream.ppm",argv[15]);screenshot(path);}
        click("[1]");assert_touch_targets();
        if(argc>15 && (i==0 || i==5)){char path[512];snprintf(path,sizeof(path),i==0?"%s/music-intro-sources-dark.ppm":"%s/music-intro-sources-cream.ppm",argv[15]);screenshot(path);}click("Back");}
    click("Back");
    // Exercise actual LVGL Retry targets for all three topics, without a network
    // worker. The service uses the same native parser/cache core as production.
    for(const char *topic:{"Artist","Release","Song"}){
        orbit_ui_intro_mock_scenario(1);click(topic);assert(has_label(lv_scr_act(),"Loading"));advance(1700);
        assert(has_label(lv_scr_act(),"Could not load introduction"));
        auto *retry=button(lv_scr_act(),"Retry in 5s");assert(retry&&lv_obj_has_state(retry,LV_STATE_DISABLED));assert(lv_obj_get_style_opa(retry,0)>=LV_OPA_70);
        assert(orbit_ui_intro_mock_starts()==1);
        if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-retry.ppm",argv[15]);screenshot(path);}
        advance(5100);assert(button(lv_scr_act(),"Retry"));click("Retry");assert(has_label(lv_scr_act(),"Loading"));advance(1700);
        assert(has_label(lv_scr_act(),"Local sample")&&orbit_ui_intro_mock_starts()==2);
        click("Back");click(topic);assert(has_label(lv_scr_act(),"Local sample | Saved")&&orbit_ui_intro_mock_starts()==2);
        if(argc>15){char path[512];snprintf(path,sizeof(path),"%s/music-intro-saved.ppm",argv[15]);screenshot(path);}
        click("Back");
    }
    orbit_ui_intro_mock_scenario(0);click("Artist");assert(has_label(lv_scr_act(),"Loading"));click("Back");advance(2000);
    assert(button(lv_scr_act(),"Artist")&&!reading_view(lv_scr_act()));click("Artist");assert(has_label(lv_scr_act(),"Loading"));advance(1700);
    assert(orbit_ui_intro_mock_starts()==2&&has_label(lv_scr_act(),"Local sample"));click("Back");
    // Temporary disconnect keeps completed content; confirmed changes while asleep release it.
    click("Artist");assert(has_label(lv_scr_act(),"Local sample | Saved"));
    const auto retainedStarts=orbit_ui_intro_mock_starts();s3::next.connected=false;s3::pending=true;advance(200);
    s3::next.connected=true;s3::pending=true;advance(200);click("Back");click("Artist");assert(has_label(lv_scr_act(),"Local sample | Saved")&&orbit_ui_intro_mock_starts()==retainedStarts);click("Back");
    orbit_ui_intro_mock_scenario(0);click("Song");assert(has_label(lv_scr_act(),"Loading"));
    orbit_ui_set_screen_awake(false);strcpy(s3::next.title,"Changed while display sleeps");s3::pending=true;advance(200);advance(2000);
    orbit_ui_set_screen_awake(true);assert(has_label(lv_scr_act(),"Track changed")&&!reading_view(lv_scr_act()));
    assert(orbit_ui_intro_mock_starts()==1);click("Back");assert(has_label(lv_scr_act(),"Changed while display sleeps"));
    click("Song");assert(has_label(lv_scr_act(),"Loading"));advance(1700);assert(orbit_ui_intro_mock_starts()==2);click("Back");click("Back");assert(button(lv_scr_act(),"I/O"));
    s3::next.input=1;s3::pending=true;advance(200);assert(!button(lv_scr_act(),"Music information"));
    s3::next.input=0;s3::next.connected=false;s3::pending=true;advance(200);assert(!button(lv_scr_act(),"Music information"));
    assert(s3::writes==introWrites&&s3::volume_writes==introVolumeWrites);
    puts("PASS: radial Artist/Release/Song targets, actual continuous vertical LVGL scroll during contact, reverse/bounds/end-over-Back, horizontal return, captured context, UTF-8,8 themes/idle; no audio writes");
    puts("PASS: conditional lyric badge/cover-vinyl hit/seek-pause-clock synchronization/CJK-wrap/idle/tap-return/stale-track cleanup, no audio writes; actual weather snapshot UI/8 palettes/conditions/cache/offline/idle, no fabricated values;280ms whole-page exit/entry, uniform ring/header/source/footer/transport/artwork displacement, actual moving pixels/settled geometry/no scale, full-contact input guard, idle/standby cancellation,20 repeated cycles; network clock offline/sync/ready, Wi-Fi vs S3 independence, idle and directional markers; intent-first pending/tap/drag, lower/header enabled-disabled button swipes, ring tap/jitter/drag and no audio misfires; four swipe pages/real cover and button arbitration/diagonal and timeout/blank idle/standby; native LVGL UI, I/O About/Theme navigation/back and IP/status values, offline themes, 100 palette switches, UTF-8 labels and actual long-artist scroll/unchanged-poll/short-name/idle behavior, clean record center and rotating highlights, larger disjoint touch targets, icon-only mute/header feedback and read-only +/-, original chain/orbit footer assets and verified readback feedback and circular constant-style mute; cover/fallback lifecycle, detailed artwork cache, reversible open/close animation, idle settlement, same-revision/track/standby cleanup and play/pause rotation; no actual network writes");
}
