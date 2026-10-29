#include "orbit_ui.h"
#include "config.h"
#include "intro_ai_runtime.h"
#include "firmware_version.h"
#include "metadata_font.h"
#include "metadata_text.h"
#include "s3_client.h"
#include "artwork_decoder.h"
#include "source_icons.h"
#include "intro_icons.h"
#if ORBIT_INTRO_ASYNC
extern "C" {
#include "src/extra/libs/qrcode/qrcodegen.h"
}
#endif
#include "status_icons.h"
#include "status_feedback.h"
#include "playback_progress.h"
#include "source_status.h"
#include "ui_themes.h"
#include "appearance_settings.h"
#include "orbit_display.h"
#include "button_layout.h"
#include "touch_regions.h"
#include "swipe_gesture.h"
#include "network_clock.h"
#include "pixel_clock_font.h"
#include "time_tools.h"
#include "iphone_trigger.h"
#include "airplay_launch_icon.h"
#include "weather.h"
#include "weather_icon.h"
#include "lyrics.h"
#include "lyrics_parser.h"
#include "lyrics_icon.h"
#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
LV_FONT_DECLARE(orbit_metadata_24);
LV_FONT_DECLARE(orbit_metadata_12);
}

namespace {
constexpr uint32_t BG=0x0d1515, FG=0xe6eeea, MUTED=0x8d9f97;
constexpr uint32_t GREEN=0xc6dfcb, TRACK=0x2b3734;
enum View { PLAYER, VOLUME, ROUTING, THEME, ABOUT, LYRICS, INTRO, INTRO_READ, INTRO_SOURCES, ALARM_EDIT, TIMER_EDIT, CLOCK_ALERT, BLANK_UP, BLANK_DOWN, BLANK_LEFT, BLANK_RIGHT, VIEW_COUNT };
bool blank_view(View v){return (v>=BLANK_UP && v<=BLANK_RIGHT) || v==ALARM_EDIT || v==TIMER_EDIT || v==CLOCK_ALERT;}
enum Action { SHOW_VOLUME=1, SHOW_ROUTES, BACK, PLAY, PREVIOUS, NEXT,
              MUTE, MINUS, PLUS, SHOW_THEME, THEME_BACK, SHOW_ABOUT, ABOUT_BACK, POWER_STANDBY, POWER_WAKE, ARTWORK_TOGGLE, BRIGHTNESS_MINUS, BRIGHTNESS_PLUS, LAUNCH_IPHONE,
              INPUT_BASE=20, OUTPUT_BASE=40, THEME_BASE=60, SHOW_LYRICS=80, CLOSE_LYRICS=81, CLOCK_TIME=90,CLOCK_ALARM,CLOCK_TIMER,ALARM_TOGGLE,EDIT_HOUR,EDIT_MINUTE,EDIT_MINUS,EDIT_PLUS,ALARM_SAVE,ALARM_DRAFT_TOGGLE,TIMER_RUN,TIMER_RESET,ALERT_STOP, SHOW_INTRO=110, INTRO_BACK, INTRO_TOPIC_BASE, INTRO_RETRY=118, INTRO_SOURCE_BACK=120, INTRO_SOURCE_BASE=124 };
struct Track { const char *title; const char *artist; const char *album; int duration; bool has_cover; };
const Track tracks[]={{"A Place Between","Northline","Still / Moving",258,true},
                      {"Soft Landing","Northline","Still / Moving",224,false},
                      {"After the Rain","Northline","Blue Hours",301,true}};
const char *inputs[]={"Network","Bluetooth","Line in","HDMI","Optical"};
struct State {
    View view=PLAYER, about_return=PLAYER;
    int volume=38, input=0, output=0, track=0, elapsed=84;
    bool muted=false, playing=true, network_active=true, standby=false;
} state;
s3::Snapshot remote;
s3::PlaybackProgress playbackProgress;
lv_obj_t *volume_buttons[2], *mute_buttons[2];
uint32_t pending_volume_at=0;
bool pending_volume=false;
lv_font_t metadata_title_font,metadata_artist_font;
uint8_t metadata_title_top=0,metadata_artist_top=0;
bool metadata_dirty=true,metadata_demo_active=false;int metadata_demo_track=-1;
int volume_max() {
    if(!ORBIT_LIVE || remote.volumeMax<=0) return 100;
    return remote.volumeMax;
}
int volume_cap() {
    if(!ORBIT_LIVE)return 100;
    int cap=remote.volumeMax;
    if(remote.volumeCeiling<cap)cap=remote.volumeCeiling;
    if(!remote.volumeLimitKnown)return -1;
    return remote.volumeLimit<cap?remote.volumeLimit:cap;
}
bool media_blocked();
bool volume_available() { return !media_blocked() && (!ORBIT_LIVE || (remote.connected && remote.volumeKnown && remote.volumeMax>0 && volume_cap()>=0)); }
uint32_t volume_limit_until=0;bool volume_limit_feedback=false;
lv_obj_t *root, *pages[VIEW_COUNT], *ring, *chip, *chip_text, *source, *source_row, *source_icon, *route_source_icon;
lv_obj_t *theme_buttons[orbit_theme::Count], *theme_swatches[orbit_theme::Count];
unsigned theme_index=0;
lv_obj_t *brightness_value,*brightness_buttons[2],*appearance_hint;
lv_obj_t *about_values[8], *about_status;
lv_obj_t *standby_button,*wake_button,*home_io;
lv_obj_t *about_input_key;
constexpr uint32_t PIXEL_YELLOW=0xffe64a;
lv_obj_t *clock_date,*clock_zone,*clock_status,*clock_time,*clock_seconds,*clock_alarm_caption,*clock_alarm_value,*clock_alarm_toggle;
lv_obj_t *alarm_fields[2],*timer_fields[2],*edit_selection[2],*alarm_draft_toggle,*timer_run,*clock_alert_title;
lv_obj_t *clock_save_hint[2],*edit_minus[2],*edit_plus[2];
unsigned alarm_hour=7,alarm_minute=30,edit_field[2]={0,0};bool alarm_enabled=false;
uint32_t clock_poll_at=0;orbit_clock::Reading alarm_wall;

orbit_clock::Reading clock_reading;
lv_obj_t *intro_entry,*intro_subject,*intro_context,*intro_choices[3],*intro_read_subject,*intro_read_kind,*intro_body,*intro_viewport;
struct IntroContext {char title[160]={},artist[160]={},album[160]={};uint32_t duration=0;} intro_context_data;
unsigned intro_topic=0;
#if ORBIT_INTRO_ASYNC
orbit_intro_async::UiClient *intro_client=nullptr;
lv_obj_t *intro_loading,*intro_message,*intro_retry,*intro_feedback;
lv_obj_t *intro_source_heading,*intro_paragraph2,*intro_references[4],*intro_reference_titles[4],*intro_source_title,*intro_source_domain,*intro_qr,*intro_source_tabs[4],*intro_source_error;
uint8_t *intro_qr_data=nullptr;
unsigned intro_source_index=0;
uint32_t intro_frame=~0u,intro_wait=~0u;
#endif
const char *intro_names[]={"Artist","Release","Song"};
// Explicit layout previews, never generated biography/facts about a real artist.
const char *intro_copy[3]={
"A short portrait of the artist or band: their background, influences, and sound.\n\nThe people and ideas behind the music, told in a few clear paragraphs.\n\nA useful introduction adds a little context, then leaves room for listening. It can point to a recording, a collaboration, or a turning point worth hearing for yourself.",
"The context of this release: its setting, sound, and place in the artist's work.\n\nAn album, EP, single, or live recording can capture a different moment in an artist's work.\n\nA short introduction can explore the recording, its performances, and the ideas that connect it to the music around it.",
"The story of this song: its sound, writing, and the details worth listening for.\n\nA closer listen to the words, instruments, and moments that make it distinctive.\n\nA brief introduction can offer another way into the recording. The aim is to add context without interrupting the music or turning a small screen into a long essay."
};
lv_obj_t *lyrics_button,*lyrics_viewport,*lyrics_rows[5];
orbit_lyrics::Document *lyrics_document=nullptr;
uint32_t lyrics_generation=0;int lyrics_cursor=-999;unsigned lyrics_theme=~0u;
lv_obj_t *weather_city,*weather_icon,*weather_temperature,*weather_condition,*weather_range,*weather_details,*weather_wind,*weather_age,*weather_humidity,*weather_precip;
lv_obj_t *weather_metric_labels[3];
orbit_weather::Icon weather_shape=orbit_weather::Icon::Sun;bool weather_day=true;
lv_obj_t *iphone_button,*iphone_status;
bool iphone_waking=false;uint32_t iphone_wake_at=0,iphone_error_at=0;
const char *iphone_error=nullptr;
lv_obj_t *status_icon, *status_target;
orbit_status::Feedback statusFeedback;
// Baseline colors are semantic tokens; artwork pixels are never tinted.
lv_color_t themed_color(uint32_t baseline) {
    for(unsigned i=0;i<orbit_theme::COLOR_COUNT;i++)
        if(baseline==orbit_theme::palettes[0].colors[i])
            return lv_color_hex(orbit_theme::palettes[theme_index].colors[i]);
    return lv_color_hex(baseline);
}
lv_obj_t *title, *artist, *cover, *vinyl, *transport[3], *play_text;
lv_obj_t *progress, *elapsed, *duration, *number, *volume_unit, *mute_text, *volume_hint, *mute_slash;
lv_obj_t *input_btns[5], *output_btns[2], *route_source;
lv_color_t *cover_pixels=nullptr;
lv_img_dsc_t cover_image;
lv_img_dsc_t live_cover_image;
s3::Artwork live_artwork;
bool live_cover_ready=false;
constexpr unsigned ArtworkViewSide=s3::ArtworkLargeSide;
constexpr size_t ArtworkViewBytes=ArtworkViewSide*ArtworkViewSide*sizeof(lv_color_t);
lv_obj_t *artwork_view,*artwork_large;
lv_color_t *demo_large_pixels=nullptr;
int artwork_progress=0,artwork_target=0;
lv_img_dsc_t artwork_view_image;
bool artwork_zoomed=false,artwork_pressed=false;
uint32_t artwork_view_revision=0;
int artwork_view_track=-1;
lv_color_t *vinyl_pixels=nullptr;
lv_img_dsc_t vinyl_image;
uint32_t vinyl_phase=0; // milliseconds within one 18-second revolution
bool vinyl_visible=false;
lv_obj_t *vinyl_shine=nullptr,*tonearm=nullptr;
int arm_position=0,arm_from=0,arm_to=0;
uint32_t arm_started=0;

bool screen_awake=true;
bool dragging=false;
float last_angle=0, gesture_volume=38;
orbit_touch::SwipeGesture swipe;
View swipe_origin=PLAYER;
enum class ContactOwner : uint8_t {Pending,Page,Volume,Reading,Blocked};
ContactOwner contact_owner=ContactOwner::Pending;
lv_point_t contact_start={0,0};
bool swipe_zoomed=false,swipe_sleeping=false,contact_can_write=false;
// One animation at a time; all root children share one offset, canvas stays fixed.
lv_obj_t *slide_page=nullptr;
View slide_target=PLAYER;
bool slide_horizontal=false,slide_contact_blocked=false,slide_outgoing=false;
int8_t slide_direction=1;
constexpr int PageSlideOutDistance=48,PageSlideInDistance=64;
constexpr int PageSlideOutDuration=100,PageSlideInDuration=180;


void render();
void render_progress();
void render_intro();
void render_lyrics();
void clear_lyrics();
bool lyrics_available();
void lyrics_scroll(void *,int32_t);
void apply_theme(unsigned index);
void show(View v);
void stop_page_slide();
void consume_swipe_contact();
void click(lv_event_t *e);
void source_image(lv_obj_t *image,s3::SourceIcon icon) {
    auto *asset=s3::source_icon_image(icon);
    if(!asset){lv_obj_add_flag(image,LV_OBJ_FLAG_HIDDEN);return;}
    lv_obj_clear_flag(image,LV_OBJ_FLAG_HIDDEN);
    if(lv_img_get_src(image)!=asset) lv_img_set_src(image,asset);
}
bool power_busy() {
    return ORBIT_LIVE && (remote.powerPending || remote.powerTransition ||
        (statusFeedback.pending && s3::power_action(statusFeedback.action)));
}
bool sleeping() {return ORBIT_LIVE?(remote.connected && remote.power==s3::PowerState::Standby):state.standby;}
bool media_blocked() {return sleeping() || power_busy() || (ORBIT_LIVE && s3::power_blocks_media(remote));}
bool playable() {
    const bool bt_unlinked=remote.input==1 && remote.bluetooth!=s3::BluetoothStatus::Unknown &&
        remote.bluetooth!=s3::BluetoothStatus::Connected;
    return !media_blocked() && (ORBIT_LIVE?remote.connected && remote.playerKnown && remote.sourceKnown &&
        remote.input>=0 && remote.input<=1 && !bt_unlinked:state.input==0 && state.network_active);
}
void text(lv_obj_t *o,const char *value) {
    // Keep title/artist scrolling and text allocations intact on unchanged polls.
    if(strcmp(lv_label_get_text(o),value)!=0) lv_label_set_text(o,value);
}
void render_metadata(const Track &track,bool active) {
    if(!ORBIT_LIVE && (metadata_demo_track!=state.track || metadata_demo_active!=active))metadata_dirty=true;
    if(!metadata_dirty)return;
    char normalized[orbit_metadata::MaxMetadataBytes];
    const char *name=ORBIT_LIVE?remote.title:(active?track.title:"");
    orbit_metadata::compose_latin(normalized,sizeof(normalized),name,strnlen(name,sizeof(normalized)-1));text(title,normalized);
    name=ORBIT_LIVE?remote.artist:(active?track.artist:"");
    orbit_metadata::compose_latin(normalized,sizeof(normalized),name,strnlen(name,sizeof(normalized)-1));text(artist,normalized);
    metadata_dirty=false;metadata_demo_track=state.track;metadata_demo_active=active;
}
void enabled(lv_obj_t *o,bool yes) {
    if(yes) lv_obj_clear_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);
    lv_obj_set_style_opa(o,yes?LV_OPA_COVER:LV_OPA_30,0);
}
void hidden(lv_obj_t *o, bool v) {
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)==v)return;
    if(v) lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,themed_color(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int w,
                const lv_font_t *font,uint32_t color=MUTED) {
    lv_obj_t *o=lv_label_create(parent);
    lv_label_set_text(o,text); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,themed_color(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);
    return o;
}
// Home, radial choices and reader headings share the proven LVGL marquee.
lv_obj_t *metadata_label(lv_obj_t *parent,int x,int y,int w,const lv_font_t *font,uint32_t color=MUTED){
    auto *o=label(parent,"",x,y,w,font,color);
    lv_label_set_long_mode(o,LV_LABEL_LONG_SCROLL_CIRCULAR);
    return o;
}
lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,
                 int action,uint32_t color=BG) {
    lv_obj_t *o=lv_btn_create(parent);
    lv_obj_remove_style_all(o); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,themed_color(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,orbit_layout::Radius,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_70,LV_STATE_PRESSED);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(o,6); // Per-grid overrides keep neighboring targets disjoint.
    auto *t=label(o,text,0,0,w,&lv_font_montserrat_18,color==GREEN?0x193225:FG);
    lv_obj_center(t);
    lv_obj_add_event_cb(o,click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(action)));
    return o;
}
lv_obj_t *pixel_label(lv_obj_t *parent,const char *value,int x,int y,int w,const lv_font_t *font=&orbit_pixel_21){return label(parent,value,x,y,w,font,PIXEL_YELLOW);}
void pixel_hit(lv_event_t *event){auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));if(!hit->res)return;auto *o=lv_event_get_target(event);lv_area_t a;lv_obj_get_coords(o,&a);const int h=lv_obj_get_height(o),pad=h<60?(60-h)/2:0,xpad=h==36?0:4;const int x=hit->point->x,y=hit->point->y;hit->res=x>=a.x1-xpad && x<=a.x2+xpad && y>=a.y1-pad && y<=a.y2+pad && (x-240)*(x-240)+(y-240)*(y-240)<=240*240;}
lv_obj_t *pixel_button(lv_obj_t *parent,const char *value,int x,int y,int w,int h,int action,const lv_font_t *font=&orbit_pixel_21){
    auto *b=button(parent,value,x,y,w,h,action,0x000000);lv_obj_set_style_radius(b,0,0);lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(b,LV_OPA_COVER,LV_STATE_PRESSED);lv_obj_set_style_bg_color(b,lv_color_hex(0x37300b),LV_STATE_PRESSED);lv_obj_set_ext_click_area(b,12);lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,pixel_hit,LV_EVENT_HIT_TEST,nullptr);
    auto *t=lv_obj_get_child(b,0);lv_obj_set_style_text_font(t,font,0);lv_obj_set_style_text_color(t,lv_color_hex(PIXEL_YELLOW),0);lv_obj_center(t);return b;
}
void pixel_selected(lv_obj_t *b,bool yes){lv_obj_set_style_bg_color(b,lv_color_hex(yes?PIXEL_YELLOW:0x000000),0);lv_obj_set_style_bg_opa(b,yes?LV_OPA_COVER:LV_OPA_TRANSP,0);lv_obj_set_style_text_color(lv_obj_get_child(b,0),lv_color_hex(yes?0x000000:PIXEL_YELLOW),0);}
void pixel_button_text(lv_obj_t *b,const char *value){text(lv_obj_get_child(b,0),value);lv_obj_center(lv_obj_get_child(b,0));}
void clock_tabs(lv_obj_t *page,int active){const char *names[]={"TIME","ALRM","TMR"};const int actions[]={CLOCK_TIME,CLOCK_ALARM,CLOCK_TIMER};for(int i=0;i<3;++i){auto *b=pixel_button(page,names[i],104+92*i,400,88,36,actions[i]);pixel_selected(b,i==active);}}

bool has_cover() {
    if(sleeping())return false;
    if(!ORBIT_LIVE)return playable() && tracks[state.track].has_cover && cover_pixels;
    return live_cover_ready && live_artwork.data && live_artwork.revision==remote.artworkRevision &&
        !strcmp(live_artwork.url,remote.artwork);
}
void chip_cover_hit(lv_event_t *event) {
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));
    if(!hit->res || state.view!=PLAYER)return;
    lv_area_t area;
    if(lyrics_button&&!lv_obj_has_flag(lyrics_button,LV_OBJ_FLAG_HIDDEN)){
        lv_obj_get_click_area(lyrics_button,&area);if(_lv_area_is_point_on(&area,hit->point,0)){hit->res=false;return;}
    }
    if(has_cover()){lv_obj_get_coords(cover,&area);if(_lv_area_is_point_on(&area,hit->point,0))hit->res=false;}
}
void artwork_frame(void *,int32_t value);
void drop_artwork_view() {
    lv_anim_del(artwork_large,artwork_frame);
    // If an external track/power change closes a held viewer, consume release.
    if(artwork_pressed)for(auto *i=lv_indev_get_next(nullptr);i;i=lv_indev_get_next(i))
        if(lv_indev_get_type(i)==LV_INDEV_TYPE_POINTER)lv_indev_wait_release(i);
    artwork_pressed=false;artwork_zoomed=false;
    hidden(artwork_view,true);hidden(artwork_large,true);
    lv_img_cache_invalidate_src(&artwork_view_image);
    lv_img_set_src(artwork_large,LV_SYMBOL_AUDIO);
    artwork_progress=artwork_target=0;artwork_view_image={};
}
void artwork_contact(lv_event_t *event) {
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED)artwork_pressed=true;
    else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST)artwork_pressed=false;
}
// Short centered crossfade: no moving/zoomed RGB565 software transform.
// Per-image opacity uses LVGL's direct blend path, not a full-screen layer.
void artwork_frame(void *,int32_t value) {
    artwork_progress=value;
    lv_obj_set_style_img_opa(artwork_large,255*value/1000,0);
    lv_obj_set_style_bg_opa(artwork_view,255*value/1000,0);
}
void artwork_finished(lv_anim_t *) {
    if(artwork_target==0){drop_artwork_view();show(PLAYER);}
    else {
        hidden(pages[PLAYER],true);hidden(ring,true);hidden(chip,true);hidden(source_row,true);
        hidden(lv_obj_get_parent(status_icon),true);hidden(status_target,true);
    }
}
void animate_artwork(bool open) {
    lv_anim_del(artwork_large,artwork_frame);
    artwork_target=open?1000:0;
    // Closing restores the current player underneath the fading backdrop.
    hidden(pages[PLAYER],false);hidden(ring,false);hidden(chip,false);hidden(source_row,false);
    hidden(lv_obj_get_parent(status_icon),false);hidden(status_target,false);hidden(cover,false);
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,artwork_large);
    lv_anim_set_exec_cb(&a,artwork_frame);lv_anim_set_values(&a,artwork_progress,artwork_target);
    int distance=abs(artwork_target-artwork_progress);
    const int remaining=(open?220:140)*distance/1000;
    lv_anim_set_time(&a,distance?(remaining<40?40:remaining):1);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);lv_anim_set_ready_cb(&a,artwork_finished);
    if(!lv_anim_start(&a)){artwork_frame(nullptr,artwork_target);artwork_finished(nullptr);}
}
void open_artwork_view() {
    if(!screen_awake || state.view!=PLAYER || artwork_zoomed || !has_cover())return;
    const auto *pixels=ORBIT_LIVE?live_artwork.large:reinterpret_cast<const uint8_t*>(demo_large_pixels);
    if(!pixels || (ORBIT_LIVE && live_artwork.largeSize!=s3::ArtworkLargeBytes))return;
    artwork_view_image={};
    artwork_view_image.header.cf=LV_IMG_CF_TRUE_COLOR;
    artwork_view_image.header.w=artwork_view_image.header.h=ArtworkViewSide;
    artwork_view_image.data_size=ArtworkViewBytes;artwork_view_image.data=pixels;
    lv_img_set_src(artwork_large,&artwork_view_image);
    // set_src resets the image pivot. Keep a fixed native-size centered image.
    lv_img_set_pivot(artwork_large,0,0);lv_img_set_zoom(artwork_large,256);
    lv_obj_set_pos(artwork_large,80,80);hidden(artwork_large,false);
    artwork_view_revision=remote.artworkRevision;artwork_view_track=state.track;
    artwork_zoomed=true;dragging=false;artwork_progress=0;artwork_frame(nullptr,0);
    hidden(artwork_view,false);lv_obj_move_foreground(artwork_view);animate_artwork(true);
}

void side_hit(lv_event_t *event) {
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));
    const int dx=hit->point->x-240,dy=hit->point->y-240;
    // Keep the enlarged mute target inside the outer-ring gesture lane.
    if(dx*dx+dy*dy>=205*205)hit->res=false;
}
void selected(lv_obj_t *b,bool yes) {
    lv_obj_set_style_bg_color(b,themed_color(yes?GREEN:0x17221d),0);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(b);i++) {
        auto *child=lv_obj_get_child(b,i);
        if(lv_obj_check_type(child,&lv_img_class))
            lv_obj_set_style_img_recolor(child,themed_color(yes?0x193225:MUTED),0);
        else lv_obj_set_style_text_color(child,themed_color(yes?0x193225:MUTED),0);
    }
}
// Compact choice rows keep a stable leading icon column; text blocks are
// vertically centered as a whole, including an optional secondary line.
lv_obj_t *route_button(lv_obj_t *parent,const char *symbol,const char *text,
                       const char *detail,int x,int y,int action,int h,const lv_img_dsc_t *asset=nullptr) {
    using namespace orbit_layout;
    auto *b=button(parent,"",x,y,124,h,action,0x17221d);
    lv_obj_set_ext_click_area(b,12);lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,pixel_hit,LV_EVENT_HIT_TEST,nullptr);
    const int title_h=lv_font_montserrat_14.line_height;
    const int detail_h=detail?lv_font_montserrat_12.line_height:0;
    const int top=centered(h,text_height(title_h,detail_h));
    auto *name=lv_obj_get_child(b,0);
    lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(name,text_left(),top);lv_obj_set_width(name,text_width(124));
    lv_label_set_text(name,text);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(name,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
    if(asset) {
        auto *image=lv_img_create(b);lv_img_set_src(image,asset);
        // The sparse RCA/XLR artwork keeps its accepted26px optical size,
        // with1px overhang either side of the common24px alignment slot.
        lv_obj_set_pos(image,ChoicePad+centered(IconSlot,int(asset->header.w)),centered(h,int(asset->header.h)));
        lv_obj_set_style_img_recolor(image,themed_color(MUTED),0);
        lv_obj_set_style_img_recolor_opa(image,LV_OPA_COVER,0);
        lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE);
    } else label(b,symbol,ChoicePad,centered(h,lv_font_montserrat_18.line_height),IconSlot,&lv_font_montserrat_18);
    if(detail) {
        auto *sub=label(b,detail,text_left(),top+title_h+DetailGap,text_width(124),&lv_font_montserrat_12);
        lv_obj_set_style_text_align(sub,LV_TEXT_ALIGN_LEFT,0);
        lv_label_set_long_mode(sub,LV_LABEL_LONG_DOT);
    }
    return b;
}
// Single actions center the measured icon+label group, rather than a full-width
// text label with spaces used as padding. Child0 remains the accessible label.
void footer_hit(lv_event_t *event) {
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));
    hit->res=hit->res && orbit_touch::footer(hit->point->x,hit->point->y);
}
void back_hit(lv_event_t *event) {
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));
    lv_area_t area;lv_obj_get_coords(lv_event_get_target(event),&area);
    const int above=static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    const int x=hit->point->x,y=hit->point->y,dx=x-240,dy=y-240;
    // Wider below/alongside the label; keep the upper neighboring controls free.
    hit->res=hit->res && y>=area.y1-above && x>=0 && x<480 && y>=0 && y<480 &&
        dx*dx+dy*dy<=240*240;
}
lv_obj_t *back_button(lv_obj_t *parent,int x,int y,int h,int action) {
    using namespace orbit_layout;
    auto *b=button(parent,"Back",x,y,152,h,action);
    lv_obj_set_ext_click_area(b,32);
    lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(b,back_hit,LV_EVENT_HIT_TEST,
        reinterpret_cast<void*>(static_cast<intptr_t>(action==THEME_BACK?20:10)));
    auto *name=lv_obj_get_child(b,0);
    const int tw=lv_txt_get_width("Back",4,&lv_font_montserrat_18,0,LV_TEXT_FLAG_NONE);
    const int left=centered(152,ActionIcon+Gap+tw);
    lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(name,left+ActionIcon+Gap,centered(h,lv_font_montserrat_18.line_height));
    lv_obj_set_width(name,tw);lv_label_set_long_mode(name,LV_LABEL_LONG_CLIP);
    label(b,LV_SYMBOL_LEFT,left,centered(h,lv_font_montserrat_18.line_height),ActionIcon,&lv_font_montserrat_18,FG);
    return b;
}
void volume_symbol(lv_obj_t *b,bool plus) {
    // Keep the control label/identity; render math signs with equal visible
    // length and an optical center, independent of a typeface's hyphen metrics.
    hidden(lv_obj_get_child(b,0),true);
    static const lv_point_t horizontal[]={{0,0},{26,0}};
    static const lv_point_t vertical[]={{0,0},{0,26}};
    for(int axis=0;axis<(plus?2:1);++axis) {
        auto *line=lv_line_create(b);
        lv_line_set_points(line,axis?vertical:horizontal,2);
        lv_obj_set_pos(line,axis?36:23,axis?23:36);
        lv_obj_set_style_line_width(line,3,0);
        lv_obj_set_style_line_rounded(line,true,0);
        lv_obj_set_style_line_color(line,themed_color(FG),0);
        lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
}
void intro_entry_hit(lv_event_t *event){
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));if(!hit->res)return;
    // The expanded I/O/mute/transport targets keep priority over metadata corners.
    for(auto *control:{home_io,mute_buttons[0],transport[0],transport[1],transport[2]})
        if(control && lv_obj_hit_test(control,hit->point)){hit->res=false;return;}
}
bool intro_text(const char *s){for(;*s;++s)if(static_cast<unsigned char>(*s)>32)return true;return false;}
bool intro_available(){
    if(media_blocked())return false;
    if(!ORBIT_LIVE)return playable();
    return remote.connected && remote.sourceKnown && remote.playerKnown && remote.input==0 &&
        !playbackProgress.changing(lv_tick_get()) &&
        (intro_text(remote.title)||intro_text(remote.artist)||intro_text(remote.album));
}
const char *intro_topic_subject(unsigned topic){return topic==0?intro_context_data.artist:topic==1?intro_context_data.album:intro_context_data.title;}
// A release may be reported, or require a later identity lookup. Never treat the
// song title as an album name or invent a release when metadata is absent.
bool intro_needs_release_identification(){return !intro_text(intro_context_data.album) && intro_text(intro_context_data.title) && intro_text(intro_context_data.artist);}
bool intro_topic_enabled(unsigned topic){return intro_text(intro_context_data.artist) && (topic==0 || intro_text(intro_topic_subject(topic)) || (topic==1 && intro_needs_release_identification()));}

bool intro_captured=false,intro_stale=false;int intro_track_index=-1;
void capture_intro(){
    intro_captured=true;intro_stale=false;intro_track_index=state.track;
    const auto &track=tracks[state.track];
    const char *fields[]={ORBIT_LIVE?remote.title:track.title,ORBIT_LIVE?remote.artist:track.artist,ORBIT_LIVE?remote.album:track.album};
    char *dest[]={intro_context_data.title,intro_context_data.artist,intro_context_data.album};
    for(unsigned i=0;i<3;++i)orbit_metadata::compose_latin(dest[i],160,fields[i],strnlen(fields[i],159));
    intro_context_data.duration=uint32_t((ORBIT_LIVE?remote.duration:track.duration)>0?(ORBIT_LIVE?remote.duration:track.duration):0);
    intro_topic=0;
}
void observe_intro_track(){
    if(!intro_captured || intro_stale)return;
    if(ORBIT_LIVE && (!remote.connected || !remote.sourceKnown || (remote.input==0 && (!remote.playerKnown || !intro_text(remote.title) || !intro_text(remote.artist)))))return;
    const auto &t=tracks[state.track];
    bool changed=!ORBIT_LIVE && intro_track_index!=state.track;
    if(ORBIT_LIVE && remote.input!=0)changed=true;
    const char *fields[]={ORBIT_LIVE?remote.title:t.title,ORBIT_LIVE?remote.artist:t.artist,ORBIT_LIVE?remote.album:t.album};
    const char *captured[]={intro_context_data.title,intro_context_data.artist,intro_context_data.album};
    for(unsigned i=0;i<3;++i){char normalized[160];orbit_metadata::compose_latin(normalized,sizeof normalized,fields[i],strnlen(fields[i],159));if(strcmp(normalized,captured[i]))changed=true;}
    const int duration=ORBIT_LIVE?remote.duration:t.duration;
    if(duration>0 && intro_context_data.duration && uint32_t(duration)!=intro_context_data.duration)changed=true;
    if(!changed)return;
    intro_stale=true;
#if ORBIT_INTRO_ASYNC
    if(intro_client)intro_client->trackChanged();
#endif
}
void intro_scrolling(){
    lv_obj_t *subjects[]={intro_subject,intro_context,intro_read_subject};
    for(unsigned i=0;i<3;++i){if(!subjects[i])continue;const bool visible=screen_awake && state.view==(i<2?INTRO:INTRO_READ);const auto mode=visible?LV_LABEL_LONG_SCROLL_CIRCULAR:LV_LABEL_LONG_DOT;
        if(lv_label_get_long_mode(subjects[i])!=mode)lv_label_set_long_mode(subjects[i],mode);}
}
bool intro_reader(View v){return v==INTRO_READ || v==INTRO_SOURCES;}
void stop_intro_motion(){if(intro_viewport)lv_obj_scroll_to_y(intro_viewport,lv_obj_get_scroll_y(intro_viewport),LV_ANIM_OFF);}
bool intro_read_contact(lv_point_t point){if(lv_obj_has_flag(intro_viewport,LV_OBJ_FLAG_HIDDEN))return false;lv_area_t a;lv_obj_get_coords(intro_viewport,&a);return _lv_area_is_point_on(&a,&point,0);}
#if ORBIT_INTRO_ASYNC
void begin_intro_request(){
    if(!intro_client)intro_client=orbit_intro_runtime::client();
    if(!intro_client)return;
    orbit_intro_ai::Snapshot snapshot;
    snapshot.topic=static_cast<orbit_intro_ai::Topic>(intro_topic);
    snprintf(snapshot.title,sizeof snapshot.title,"%s",intro_context_data.title);
    snprintf(snapshot.artist,sizeof snapshot.artist,"%s",intro_context_data.artist);
    snprintf(snapshot.release,sizeof snapshot.release,"%s",intro_context_data.album);
    snapshot.durationSeconds=intro_context_data.duration;
    intro_client->begin(snapshot,orbit_intro_runtime::nowMs());
}
#endif
#if ORBIT_INTRO_ASYNC
void clear_intro_qr(){if(intro_qr_data){lv_mem_free(intro_qr_data);intro_qr_data=nullptr;}if(intro_qr)lv_obj_invalidate(intro_qr);if(intro_source_error)lv_label_set_text_static(intro_source_error,"");}
void draw_intro_qr(lv_event_t *event){
    if(!intro_qr_data)return;
    const int modules=qrcodegen_getSize(intro_qr_data),scale=200/(modules+8),side=(modules+8)*scale;
    lv_area_t a;lv_obj_get_coords(lv_event_get_target(event),&a);
    const int x=a.x1+(200-side)/2,y=a.y1+(200-side)/2;
    auto *ctx=lv_event_get_draw_ctx(event);lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_white();
    lv_area_t paper={lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+side-1),lv_coord_t(y+side-1)};lv_draw_rect(ctx,&d,&paper);
    d.bg_color=lv_color_black();
    for(int row=0;row<modules;++row)for(int col=0;col<modules;){
        if(!qrcodegen_getModule(intro_qr_data,col,row)){++col;continue;}
        int end=col+1;while(end<modules && qrcodegen_getModule(intro_qr_data,end,row))++end;
        lv_area_t run={lv_coord_t(x+(col+4)*scale),lv_coord_t(y+(row+4)*scale),lv_coord_t(x+(end+4)*scale-1),lv_coord_t(y+(row+5)*scale-1)};
        lv_draw_rect(ctx,&d,&run);col=end;
    }
}
void prepare_intro_source(){
    clear_intro_qr();const auto *value=intro_client?intro_client->result():nullptr;
    if(!value || intro_source_index>=value->sourceCount)return;
    const auto &ref=value->sources[intro_source_index];text(intro_source_title,ref.title);
    char domain[128];size_t n=strcspn(ref.url+8,"/?#");if(n>=sizeof domain)n=sizeof domain-1;memcpy(domain,ref.url+8,n);domain[n]=0;text(intro_source_domain,domain);
    constexpr size_t bytes=qrcodegen_BUFFER_LEN_FOR_VERSION(16);
    auto *data=static_cast<uint8_t*>(lv_mem_alloc(bytes));auto *temp=static_cast<uint8_t*>(lv_mem_alloc(bytes));
    if(data && temp && qrcodegen_encodeText(ref.url,temp,data,qrcodegen_Ecc_MEDIUM,1,16,qrcodegen_Mask_AUTO,true))intro_qr_data=data;
    else if(data)lv_mem_free(data);
    if(temp)lv_mem_free(temp);
    hidden(intro_source_error,intro_qr_data!=nullptr);lv_label_set_text_static(intro_source_error,intro_qr_data?"":ref.url);
    for(unsigned i=0;i<4;++i){hidden(intro_source_tabs[i],i>=value->sourceCount);lv_obj_set_x(intro_source_tabs[i],(480-60*value->sourceCount)/2+60*i);lv_obj_set_style_text_color(lv_obj_get_child(intro_source_tabs[i],0),themed_color(i==intro_source_index?GREEN:MUTED),0);}
    lv_obj_invalidate(intro_qr);
}
#endif
void render_intro(){
    observe_intro_track();
    if(state.view==INTRO && intro_stale && intro_available())capture_intro();
    if(state.view!=INTRO && !intro_reader(state.view))return;
    text(intro_subject,intro_text(intro_context_data.title)?intro_context_data.title:"");
    text(intro_context,intro_context_data.artist);
    for(unsigned i=0;i<3;++i)enabled(intro_choices[i],!intro_stale && intro_topic_enabled(i));
    if(state.view!=INTRO_READ)return;
    const bool identify=intro_topic==1 && intro_needs_release_identification();
    text(intro_read_kind,intro_names[intro_topic]);
    text(intro_read_subject,identify?intro_context_data.title:intro_topic_subject(intro_topic));
#if ORBIT_INTRO_ASYNC
    using orbit_intro_ai::Outcome;
    const auto outcome=intro_client?intro_client->outcome():orbit_intro_runtime::startupOutcome();
    const auto *value=intro_client?intro_client->result():nullptr;
    const bool ready=outcome==Outcome::Ready,pending=outcome==Outcome::Pending;
    hidden(intro_viewport,!ready);hidden(intro_loading,!pending);hidden(intro_message,ready);
    hidden(intro_source_heading,!ready || !value || !value->sourceCount);
    if(value){
        const unsigned split=value->paragraphStart[1];
        const unsigned count=split>=2?split-2:unsigned(strlen(value->text));
        const char *shown=lv_label_get_text(intro_body);
        // Format directly into LVGL-owned text only when the paragraph changes;
        // no1.5KiB temporary on the UI stack and no allocation on unchanged polls.
        if(strlen(shown)!=count || memcmp(shown,value->text,count))lv_label_set_text_fmt(intro_body,"%.*s",int(count),value->text);
        text(intro_paragraph2,value->text+split);
        // Plain paragraphs first; the reference list is part of the same scroll.
        lv_obj_update_layout(intro_body);int y=lv_obj_get_height(intro_body)+24;
        lv_obj_set_y(intro_paragraph2,y);lv_obj_update_layout(intro_paragraph2);
        y+=lv_obj_get_height(intro_paragraph2)+24;
        lv_obj_set_y(intro_source_heading,y);y+=26;
        for(unsigned i=0;i<4;++i){
            const bool show=ready && i<value->sourceCount;hidden(intro_references[i],!show);
            if(!show)continue;
            text(intro_reference_titles[i],value->sources[i].title);
            lv_obj_update_layout(intro_reference_titles[i]);
            const int height=lv_obj_get_height(intro_reference_titles[i])+16;
            const int rowHeight=height<60?60:height;lv_obj_set_pos(intro_references[i],12,y);lv_obj_set_height(intro_references[i],rowHeight);
            y+=rowHeight+8;
        }
    }else{text(intro_body,"");text(intro_paragraph2,"");for(auto *ref:intro_references)hidden(ref,true);}
    text(intro_message,intro_stale && outcome==Outcome::Canceled?"Track changed":outcome==Outcome::Ambiguous && intro_topic==1?"Release not identified":orbit_intro_ai::Engine::message(outcome));
    const bool retry=!intro_stale && orbit_intro_async::retryable(outcome);
    hidden(intro_retry,!retry);
    const uint32_t wait=intro_client?uint32_t((uint64_t(intro_client->retryMs(orbit_intro_runtime::nowMs()))+999)/1000):0;
    intro_wait=wait;
    enabled(intro_retry,!wait);lv_obj_set_style_opa(intro_retry,wait?LV_OPA_70:LV_OPA_COVER,0);
    char retryText[32];snprintf(retryText,sizeof retryText,wait?"Retry in %lus":"Retry",static_cast<unsigned long>(wait));
    text(lv_obj_get_child(intro_retry,0),retryText);
    #if ORBIT_INTRO_MOCK
    text(intro_feedback,ready&&intro_client->fromCache()?"Local sample | Saved":"Local sample");
#else
    text(intro_feedback,ready&&intro_client->fromCache()?"Saved":"");
#endif
    intro_frame=lv_tick_get()/150;
    if(pending){lv_img_set_src(intro_loading,orbit_status::icon_image(orbit_status::Icon::Waiting,(lv_tick_get()/150)%orbit_status::FrameCount));lv_obj_set_style_img_recolor(intro_loading,themed_color(GREEN),0);}
#else
    text(intro_body,identify?"Release not identified.":intro_copy[intro_topic]);
#endif
}
#if ORBIT_INTRO_ASYNC
void intro_reference_hit(lv_event_t *event){
    auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));lv_area_t viewport;
    lv_obj_get_coords(intro_viewport,&viewport);hit->res=hit->res && _lv_area_is_point_on(&viewport,hit->point,0);
}
#endif
void intro_radial_hit(lv_event_t *event){auto *hit=static_cast<lv_hit_test_info_t*>(lv_event_get_param(event));const int dx=hit->point->x-240,dy=hit->point->y-240;hit->res=hit->res && dx*dx+dy*dy<=240*240;}
void draw_intro_orbit(lv_event_t *event){
    auto *ctx=lv_event_get_draw_ctx(event);lv_area_t area;lv_obj_get_coords(lv_event_get_target(event),&area);
    const lv_point_t center={lv_coord_t(area.x1+240),lv_coord_t(area.y1+240)};
    lv_draw_arc_dsc_t line;lv_draw_arc_dsc_init(&line);line.color=themed_color(TRACK);line.width=2;line.rounded=1;
    // Three quiet connecting arcs leave openings for the three radial actions.
    lv_draw_arc(ctx,&line,&center,154,180,245);lv_draw_arc(ctx,&line,&center,154,295,360);lv_draw_arc(ctx,&line,&center,154,55,125);
}
void make_cover() {
    // Demo artwork is not allocated in live mode.
    cover_pixels=static_cast<lv_color_t*>(lv_mem_alloc(170*170*sizeof(lv_color_t)));
    if(!cover_pixels) return;
    demo_large_pixels=static_cast<lv_color_t*>(lv_mem_alloc(ArtworkViewBytes));
    for(unsigned variant=0;variant<2;++variant) {
    const unsigned side=variant?ArtworkViewSide:170;
    auto *dest=side==170?cover_pixels:demo_large_pixels;if(!dest)continue;
    for(unsigned py=0;py<side;py++) for(unsigned px=0;px<side;px++) {
        float x=px*170.f/side,y=py*170.f/side;
        float t=static_cast<float>(y)/170;
        int r=36+int(115*t),g=76+int(100*t),b=98+int(47*t);
        float dx=x-88,dy=y-76;
        if(dx*dx+dy*dy<36*36) {r=233;g=220;b=177;}
        if(y>104+3*sinf(x*.06f)) {r=43;g=83-int((y-104)*.45f);b=82-int((y-104)*.35f);}
        if(y>148-int(20*sinf(x*.022f))) {r=16;g=48;b=46;}
        dest[py*side+px]=lv_color_make(r,g,b);
    }
    }
    cover_image.header.always_zero=0; cover_image.header.w=170; cover_image.header.h=170;
    cover_image.header.cf=LV_IMG_CF_TRUE_COLOR;
    cover_image.data_size=170*170*sizeof(lv_color_t);
    cover_image.data=reinterpret_cast<const uint8_t*>(cover_pixels);
}
void paint_vinyl_center() {
    if(!vinyl_pixels) return;
    // Update only the 54-pixel label on explicit theme changes. Reuse its buffer.
    lv_img_cache_invalidate_src(&vinyl_image);
    for(int y=58;y<112;y++) for(int x=58;x<112;x++) {
        float dx=x-84.5f,dy=y-84.5f,r=sqrtf(dx*dx+dy*dy);
        if(r>=27) continue;
        auto c=themed_color(0x84a891);
        if(r>24 && r<26) c=themed_color(0xb0c8b3);
        if(r<5) c=themed_color(r<4?BG:0xb0c8b3);
        vinyl_pixels[y*170+x]=c;
    }
}

lv_point_t polar_point(int x,int y,int radius,int angle) {
    return {lv_coord_t(x+radius*int32_t(lv_trigo_cos(angle))/32767),
            lv_coord_t(y+radius*int32_t(lv_trigo_sin(angle))/32767)};
}
void draw_vinyl_shine(lv_event_t *event) {
    auto *ctx=lv_event_get_draw_ctx(event);lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event),&area);
    const int x=area.x1+85,y=area.y1+85,phase=33+vinyl_phase*360/18000;
    static const uint8_t alpha[]={1,4,8,14,24,38,48,40,28,16,8,3};
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_make(199,211,211);d.border_width=0;
    // Two small radial fans; static grooves are blitted, never rotated/resampled.
    for(int side=0;side<2;++side)for(int i=0;i<12;++i) {
        const int a=phase+side*180-24+i*4;
        const lv_point_t points[]={polar_point(x,y,31,a),polar_point(x,y,80,a),
                                   polar_point(x,y,80,a+4),polar_point(x,y,31,a+4)};
        d.bg_opa=side?alpha[i]*3/4:alpha[i];lv_draw_polygon(ctx,&d,points,4);
    }
}
lv_point_t arm_point(int x,int y,int dx,int dy,int angle,int lift) {
    dy=dy*(111-lift)/111;
    const int32_t cosine=lv_trigo_cos(angle),sine=lv_trigo_sin(angle);
    return {lv_coord_t(x+(dx*cosine-dy*sine)/32767),lv_coord_t(y+(dx*sine+dy*cosine)/32767)};
}
void draw_tonearm(lv_event_t *event) {
    auto *ctx=lv_event_get_draw_ctx(event);lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event),&area);
    const int x=area.x1+55,y=area.y1+12;
    const int swing=arm_position<800?arm_position*1000/800:1000;
    const int angle=-12+32*swing/1000;
    const int lift=arm_position<800?3:3*(1000-arm_position)/200;
    lv_point_t points[]={arm_point(x,y,0,0,angle,lift),arm_point(x,y,0,76,angle,lift),
                         arm_point(x,y,-6,100,angle,lift),arm_point(x,y,-6,107,angle,lift),
                         arm_point(x,y,-6,111,angle,lift)};
    // The arm is a material over a dark disc, not text on the page background.
    const bool light=lv_color_brightness(themed_color(BG))>128;
    const auto metal=light?lv_color_hex(0x6f8277):themed_color(MUTED);
    const auto highlight=light?lv_color_hex(0xe6eeea):themed_color(FG);
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.round_start=line.round_end=1;
    line.color=lv_color_black();line.width=4;line.opa=100;
    for(int i=0;i<4;++i) {
        lv_point_t a={lv_coord_t(points[i].x+2),lv_coord_t(points[i].y+2+lift/2)},
                   b={lv_coord_t(points[i+1].x+2),lv_coord_t(points[i+1].y+2+lift/2)};
        lv_draw_line(ctx,&line,&a,&b);
    }
    line.color=metal;line.width=3;line.opa=LV_OPA_COVER;
    for(int i=0;i<3;++i)lv_draw_line(ctx,&line,&points[i],&points[i+1]);
    line.color=highlight;line.width=1;line.opa=180;
    for(int i=0;i<2;++i)lv_draw_line(ctx,&line,&points[i],&points[i+1]);
    line.color=highlight;line.width=5;line.opa=LV_OPA_COVER;
    lv_draw_line(ctx,&line,&points[2],&points[3]);
    line.width=1;lv_draw_line(ctx,&line,&points[3],&points[4]);
    lv_draw_rect_dsc_t cap;lv_draw_rect_dsc_init(&cap);
    cap.radius=LV_RADIUS_CIRCLE;cap.bg_color=metal;
    cap.border_width=1;cap.border_color=themed_color(BG);
    lv_area_t bounds={lv_coord_t(x-5),lv_coord_t(y-5),lv_coord_t(x+5),lv_coord_t(y+5)};
    lv_draw_rect(ctx,&cap,&bounds);
    cap.bg_color=highlight;cap.border_width=0;
    bounds={lv_coord_t(x-1),lv_coord_t(y-1),lv_coord_t(x+1),lv_coord_t(y+1)};
    lv_draw_rect(ctx,&cap,&bounds);
}
void draw_wake_power(lv_event_t *event) {
    auto *ctx=lv_event_get_draw_ctx(event);lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event),&area);
    const lv_point_t center={lv_coord_t((area.x1+area.x2+1)/2),lv_coord_t((area.y1+area.y2+1)/2)};
    lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);
    arc.color=themed_color(0x193225);arc.width=3;arc.rounded=1;
    lv_draw_arc(ctx,&arc,&center,16,315,225);
    const lv_point_t a={center.x,lv_coord_t(center.y-18)},b={center.x,lv_coord_t(center.y-2)};
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.color=arc.color;line.width=3;line.round_start=line.round_end=1;
    lv_draw_line(ctx,&line,&a,&b);
}
void set_arm_target() {
    const int target=vinyl_visible && playable() && state.playing?1000:0;
    if(target!=arm_to) {arm_from=arm_position;arm_to=target;arm_started=lv_tick_get();}
}
void animate_tonearm() {
    if(!screen_awake || artwork_zoomed)return;
    int next=arm_to;
    if(ORBIT_ANIMATION_INTERVAL_MS && state.view==PLAYER && vinyl_visible) {
        uint32_t time=lv_tick_get()-arm_started;
        const int t=time>=600?1000:time*1000/600;
        const int ease=t*t*(3000-2*t)/1000000;
        next=arm_from+(arm_to-arm_from)*ease/1000;
    }
    if(next!=arm_position) {arm_position=next;lv_obj_invalidate(tonearm);}
}
void make_vinyl() {
    vinyl_pixels=static_cast<lv_color_t*>(lv_mem_alloc(170*170*sizeof(lv_color_t)));
    if(!vinyl_pixels) return;
    for(int y=0;y<170;y++) for(int x=0;x<170;x++) {
        float dx=x-84.5f,dy=y-84.5f,r=sqrtf(dx*dx+dy*dy);
        lv_color_t color=LV_COLOR_CHROMA_KEY;
        if(r<84) {
            // Four-pixel grooves survive RGB565 quantization at native size.
            // Generated once; playback still blits a static disc under moving shine.
            int base=12+int(10*(1-cosf(r*1.5707963f)));
            if(r>81) base=42;
            color=lv_color_make(base,base+3,base+3);
            if(r<27) color=lv_color_make(132,168,145);
            if(r>24 && r<26) color=lv_color_make(176,200,179);
            if(r<4) color=themed_color(BG);
            if(r>=4 && r<5) color=lv_color_make(185,204,189);
        }
        vinyl_pixels[y*170+x]=color;
    }
    vinyl_image.header.always_zero=0;vinyl_image.header.w=170;vinyl_image.header.h=170;
    vinyl_image.header.cf=LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
    vinyl_image.data_size=170*170*sizeof(lv_color_t);
    vinyl_image.data=reinterpret_cast<const uint8_t*>(vinyl_pixels);
    paint_vinyl_center();
}
void recolor_tree(lv_obj_t *object,unsigned previous,unsigned next) {
    // Swatches keep their own preview colors. Change only explicitly local styles.
    for(auto *swatch:theme_swatches) if(object==swatch) return;
    const lv_style_prop_t properties[]={LV_STYLE_BG_COLOR,LV_STYLE_TEXT_COLOR,
        LV_STYLE_ARC_COLOR,LV_STYLE_IMG_RECOLOR,LV_STYLE_BORDER_COLOR,LV_STYLE_LINE_COLOR};
    const lv_style_selector_t parts[]={LV_PART_MAIN,LV_PART_INDICATOR,LV_PART_KNOB,LV_PART_SCROLLBAR};
    for(auto part:parts) for(auto prop:properties) {
        lv_style_value_t value;
        if(lv_obj_get_local_style_prop(object,prop,&value,part)!=LV_STYLE_RES_FOUND) continue;
        for(unsigned i=0;i<orbit_theme::COLOR_COUNT;i++) {
            if(value.color.full!=lv_color_hex(orbit_theme::palettes[previous].colors[i]).full) continue;
            value.color=lv_color_hex(orbit_theme::palettes[next].colors[i]);
            lv_obj_set_local_style_prop(object,prop,value,part);break;
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++)
        recolor_tree(lv_obj_get_child(object,i),previous,next);
}
void render_appearance() {
    if(!brightness_value)return;
    char value[8];snprintf(value,sizeof(value),"%u%%",(orbit_appearance::brightness()*100u+8)/16);
    text(brightness_value,value);
    enabled(brightness_buttons[0],orbit_appearance::brightness()>orbit_appearance::MinBrightness);
    enabled(brightness_buttons[1],orbit_appearance::brightness()<orbit_appearance::MaxBrightness);
    text(appearance_hint,orbit_appearance::saved()?"":"Not saved");
}
void apply_theme(unsigned index) {
    if(index>=orbit_theme::Count) return;
    if(index!=theme_index) {
        recolor_tree(root,theme_index,index);theme_index=index;
        paint_vinyl_center();lv_obj_invalidate(vinyl);
    }
    lv_obj_set_style_bg_color(lv_scr_act(),themed_color(BG),0);
    lv_obj_set_style_bg_opa(lv_scr_act(),LV_OPA_COVER,0);
    for(unsigned i=0;i<orbit_theme::Count;i++) {
        lv_obj_set_style_outline_width(theme_swatches[i],i==theme_index?2:0,0);
        lv_obj_set_style_border_color(theme_swatches[i],themed_color(MUTED),0);
        lv_obj_set_style_outline_color(theme_swatches[i],themed_color(GREEN),0);
        lv_obj_set_style_text_color(lv_obj_get_child(theme_buttons[i],0),themed_color(i==theme_index?FG:MUTED),0);
    }
    render();
}
void animate_vinyl(lv_timer_t *) {
    static uint32_t last=lv_tick_get();
    const uint32_t now=lv_tick_get(),delta=now-last;last=now;
    if(!screen_awake || slide_page || !vinyl_pixels || !vinyl_visible || state.view!=PLAYER || !playable() || !state.playing) return;
    vinyl_phase=(vinyl_phase+delta)%18000;
    // Only the native light fans advance; disc and blank center stay static.
    lv_obj_invalidate(vinyl_shine);
}
void render_status() {
    using orbit_status::Icon;
    const uint32_t now=lv_tick_get();
    const auto mode=statusFeedback.current(remote,now,ORBIT_LIVE);
    // Keep feedback deadlines/detail current while avoiding hidden animation.
    if(!screen_awake || slide_page || artwork_zoomed || state.view!=PLAYER)return;
    static int previous=-1;static unsigned previous_theme=~0u;
    static uint32_t frame_at=0;static uint8_t frame=0;
    if(previous!=int(mode) || previous_theme!=theme_index) {
        previous=int(mode);previous_theme=theme_index;frame=0;frame_at=now;
        lv_img_set_src(status_icon,orbit_status::icon_image(mode));
        const uint32_t color=mode==Icon::Waiting || mode==Icon::Confirmed?GREEN:
            mode==Icon::Limited || mode==Icon::Error?FG:MUTED;
        lv_obj_set_style_img_recolor(status_icon,themed_color(color),0);
    }
    // A pre-rasterized 24px A8 mask changes at most once per 150 ms while busy.
    // No image rotation, alpha layer, extra worker/timer or allocation per frame.
    if(mode==Icon::Waiting && uint32_t(now-frame_at)>=150) {
        frame_at=now;frame=(frame+1)%orbit_status::FrameCount;
        lv_img_set_src(status_icon,orbit_status::icon_image(mode,frame));
    }
}
void volume(int v) {
    if(!volume_available()) return;
    const int cap=volume_cap();
    if(v>cap) {
        volume_limit_feedback=true;volume_limit_until=lv_tick_get()+1600;
        if(dragging)gesture_volume=cap;
    } else if(v<cap)volume_limit_feedback=false;
    int value=v<0?0:v>cap?cap:v;
    // A held ring tap and clamped +/- must not keep sending unchanged values.
    if(value==state.volume) {if(v>cap)render();return;}
    if(ORBIT_LIVE) {
        const bool sent=s3::send(s3::Action::Volume,value);
        statusFeedback.queued(s3::Action::Volume,value,remote,sent,lv_tick_get());
        if(!sent) {render_status();return;}
    }
    state.volume=value;
    if(ORBIT_LIVE) {pending_volume=true;pending_volume_at=lv_tick_get();}
    render();
}
float angle(lv_point_t p) {return atan2f(p.y-240,p.x-240)*180.0f/3.14159265f;}
void page_slide_frame(void *object,int32_t offset) {
    if(object!=slide_page)return;
    // The pages, ring, source, volume and footer are sibling layers. Move them
    // together; descendants (including transport buttons) inherit the offset.
    // Keep the opaque themed canvas stationary, no screenshots or resampling.
    for(uint32_t i=0;i<lv_obj_get_child_cnt(root);++i) {
        auto *child=lv_obj_get_child(root,i);
        if(slide_horizontal)lv_obj_set_style_translate_x(child,offset,0);
        else lv_obj_set_style_translate_y(child,offset,0);
    }
}
void stop_page_slide() {
    if(!slide_page)return;
    lv_anim_del(slide_page,page_slide_frame);
    page_slide_frame(slide_page,0);slide_page=nullptr;slide_outgoing=false;
}
void page_slide_ready(lv_anim_t *animation);
void start_page_slide(int from,int to,int duration) {
    page_slide_frame(slide_page,from);
    lv_anim_t animation;lv_anim_init(&animation);
    lv_anim_set_var(&animation,slide_page);lv_anim_set_exec_cb(&animation,page_slide_frame);
    lv_anim_set_values(&animation,from,to);lv_anim_set_time(&animation,duration);
    lv_anim_set_path_cb(&animation,slide_outgoing?lv_anim_path_ease_in:lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&animation,page_slide_ready);
    if(!lv_anim_start(&animation)) {
        const View target=slide_target;stop_page_slide();show(target);
    }
}
void page_slide_ready(lv_anim_t *animation) {
    if(animation->var!=slide_page)return;
    if(slide_outgoing) {
        const View target=slide_target;
        // LVGL removes the finished node from its list before ready_cb. Restore
        // geometry and switch synchronously, so no centered flash is painted.
        page_slide_frame(slide_page,0);slide_page=nullptr;slide_outgoing=false;
        show(target);slide_page=root;
        start_page_slide(-slide_direction*PageSlideInDistance,0,PageSlideInDuration);
    } else {
        page_slide_frame(slide_page,0);slide_page=nullptr;render();
    }
}
void show(View v) {
    if(v==ALARM_EDIT && state.view!=ALARM_EDIT){auto c=orbit_time::state().config();alarm_hour=c.hour;alarm_minute=c.minute;alarm_enabled=c.enabled;edit_field[0]=0;}
    stop_intro_motion();
#if ORBIT_INTRO_ASYNC
    if(intro_reader(state.view) && !intro_reader(v) && intro_client)intro_client->cancel();
    if(state.view==INTRO_SOURCES && v!=INTRO_SOURCES)clear_intro_qr();
#endif
    if(lyrics_viewport){lv_anim_del(lyrics_viewport,lyrics_scroll);lyrics_scroll(nullptr,0);}
    if(v==LYRICS)lyrics_cursor=-999;
    stop_page_slide();
    if(artwork_zoomed)drop_artwork_view();
    if(v==ABOUT && state.view!=ABOUT)state.about_return=state.view;
    if(v!=PLAYER && v!=VOLUME)dragging=false;
    state.view=v;intro_scrolling();
    if(ORBIT_LIVE)s3::requestBluetoothInfo(v==ABOUT);
    for(int i=0;i<VIEW_COUNT;i++) hidden(pages[i],i!=v);
    const bool overlay=v!=PLAYER && v!=VOLUME;
    hidden(chip,overlay); hidden(source_row,overlay); hidden(ring,overlay);
    hidden(lv_obj_get_parent(status_icon),v!=PLAYER);hidden(status_target,v!=PLAYER);
    render();
}
void show_swipe_page(View v,orbit_touch::SwipeDirection direction) {
    using D=orbit_touch::SwipeDirection;
    if(!screen_awake){show(v);return;}
    stop_page_slide();slide_page=root;slide_target=v;slide_outgoing=true;
    slide_horizontal=direction==D::Left || direction==D::Right;
    slide_direction=(direction==D::Left || direction==D::Up)?-1:1;
    start_page_slide(0,slide_direction*PageSlideOutDistance,PageSlideOutDuration);
}
bool iphone_s3_available() {
    return !ORBIT_LIVE || (remote.connected && !power_busy() &&
        (remote.power==s3::PowerState::Online || remote.power==s3::PowerState::Standby));
}
void fail_iphone_launch(const char *message) {
    iphone_waking=false;iphone_error=message;iphone_error_at=lv_tick_get();
}
void begin_iphone_launch() {
    if(iphone_waking || orbit_iphone::state()!=orbit_iphone::LaunchState::Ready || !iphone_s3_available())return;
    iphone_error=nullptr;
    if(ORBIT_LIVE && remote.power==s3::PowerState::Standby) {
        const bool sent=s3::send(s3::Action::Wake);
        statusFeedback.queued(s3::Action::Wake,0,remote,sent,lv_tick_get());
        if(!sent){fail_iphone_launch("Could not wake S3");return;}
        iphone_waking=true;iphone_wake_at=lv_tick_get();
        dragging=false;pending_volume=false;volume_limit_feedback=false;
    }else if(!orbit_iphone::start())fail_iphone_launch("Could not send");
}
void update_iphone_wake(bool freshSnapshot) {
    if(!iphone_waking)return;
    if(uint32_t(lv_tick_get()-iphone_wake_at)>=10000){fail_iphone_launch("Could not wake S3");return;}
    if(!freshSnapshot)return;
    if(remote.link==s3::LinkState::Offline || remote.link==s3::LinkState::Error || remote.power==s3::PowerState::Offline){
        fail_iphone_launch("Could not wake S3");return;
    }
    if(!remote.connected)return; // A transient read waits within the same deadline.
    // Only a NEW confirmed Online report may release the phone trigger. This
    // progresses with the existing UI timer even after navigation/display-off.
    if(remote.power==s3::PowerState::Online && !remote.powerPending && !remote.powerTransition && !power_busy()){
        iphone_waking=false;
        if(orbit_iphone::state()!=orbit_iphone::LaunchState::Ready || !orbit_iphone::start())fail_iphone_launch("Could not send");
    }
}
void click(lv_event_t *e) {
    if(slide_page || slide_contact_blocked)return;
    const int a=static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if(a==CLOCK_TIME){show(BLANK_UP);return;}
    if(a==CLOCK_ALARM){show(ALARM_EDIT);return;}
    if(a==CLOCK_TIMER){show(TIMER_EDIT);return;}
    if(a==ALARM_TOGGLE){auto c=orbit_time::state().config();orbit_time::alarm(c.hour,c.minute,!c.enabled);render();return;}
    if(a==ALARM_SAVE){if(orbit_time::alarm(alarm_hour,alarm_minute,alarm_enabled))show(BLANK_UP);else render();return;}
    if(a==ALARM_DRAFT_TOGGLE){alarm_enabled=!alarm_enabled;render();return;}
    if(a==ALERT_STOP){orbit_time::dismiss();show(BLANK_UP);return;}
    if(a==EDIT_HOUR || a==EDIT_MINUTE){edit_field[state.view==TIMER_EDIT?1:0]=a==EDIT_MINUTE?1:0;render();return;}
    if(a==EDIT_MINUS || a==EDIT_PLUS){
        const int delta=a==EDIT_PLUS?1:-1;
        if(state.view==ALARM_EDIT){if(edit_field[0])alarm_minute=(alarm_minute+60+delta)%60;else alarm_hour=(alarm_hour+24+delta)%24;}
        if(state.view==TIMER_EDIT && orbit_time::state().timer()!=orbit_time::Timer::Running){int seconds=int(orbit_time::state().remaining())+delta*(edit_field[1]?1:60);if(seconds<1)seconds=1;if(seconds>5999)seconds=5999;orbit_time::preset(unsigned(seconds));}
        render();return;
    }
    if(a==TIMER_RUN){if(orbit_time::state().timer()==orbit_time::Timer::Running)orbit_time::pause(lv_tick_get());else orbit_time::start(lv_tick_get());render();return;}
    if(a==TIMER_RESET){orbit_time::reset();render();return;}
    if(a==SHOW_INTRO){if(intro_available()){capture_intro();show(INTRO);}return;}
#if ORBIT_INTRO_ASYNC
    if(a==INTRO_SOURCE_BACK){show(INTRO_READ);return;}
    if(a>=INTRO_SOURCE_BASE && a<INTRO_SOURCE_BASE+4){
        const auto *value=intro_client?intro_client->result():nullptr;
        const unsigned index=a-INTRO_SOURCE_BASE;
        if(value && index<value->sourceCount){intro_source_index=index;show(INTRO_SOURCES);prepare_intro_source();}return;
    }
#endif
    if(a==INTRO_BACK){if(intro_stale){if(intro_available())capture_intro();else {show(PLAYER);return;}}show(state.view==INTRO_READ?INTRO:PLAYER);return;}
    if(a>=INTRO_TOPIC_BASE && a<INTRO_TOPIC_BASE+3){const unsigned topic=a-INTRO_TOPIC_BASE;if(state.view==INTRO && !intro_stale && intro_topic_enabled(topic)){intro_topic=topic;
#if ORBIT_INTRO_ASYNC
        begin_intro_request();
#endif
        show(INTRO_READ);lv_obj_update_layout(intro_viewport);lv_obj_scroll_to_y(intro_viewport,0,LV_ANIM_OFF);}return;}
#if ORBIT_INTRO_ASYNC
    if(a==INTRO_RETRY){if(state.view==INTRO_READ && !intro_stale && (!intro_client || intro_client->retryMs(orbit_intro_runtime::nowMs())==0)){auto o=intro_client?intro_client->outcome():orbit_intro_runtime::startupOutcome();if(orbit_intro_async::retryable(o)){begin_intro_request();render_intro();}}return;}
#endif
    if(a==SHOW_LYRICS){if(lyrics_available())show(LYRICS);return;}
    if(a==CLOSE_LYRICS){show(PLAYER);return;}
    if(a==LAUNCH_IPHONE) {
        begin_iphone_launch();
        render();return;
    }
    if(a==ARTWORK_TOGGLE) {if(artwork_zoomed)animate_artwork(artwork_target==0);else open_artwork_view();return;}
    // Local appearance/navigation works in live mode even while S3 is offline.
    if(a==SHOW_THEME) {show(THEME);return;}
    if(a==SHOW_ABOUT) {show(ABOUT);return;}
    if(a==ABOUT_BACK) {show(state.about_return);return;}
    if(a==THEME_BACK) {show(ROUTING);return;}
    if(a>=THEME_BASE && a<THEME_BASE+int(orbit_theme::Count)) {
        apply_theme(a-THEME_BASE);orbit_appearance::select_theme(a-THEME_BASE);render_appearance();return;
    }
    if(a==BRIGHTNESS_MINUS || a==BRIGHTNESS_PLUS) {
        orbit_appearance::select_brightness(int(orbit_appearance::brightness())+(a==BRIGHTNESS_PLUS?1:-1));
        orbit_display_set_brightness(orbit_appearance::brightness());render_appearance();return;
    }
    if(a==POWER_STANDBY || a==POWER_WAKE) {
        if(power_busy())return;
        if(ORBIT_LIVE) {
            const auto action=a==POWER_STANDBY?s3::Action::Standby:s3::Action::Wake;
            const bool sent=s3::send(action);
            statusFeedback.queued(action,0,remote,sent,lv_tick_get());
            if(sent) {dragging=false;pending_volume=false;volume_limit_feedback=false;state.volume=remote.volume;}
            if(sent && a==POWER_STANDBY)show(PLAYER);else render();
        } else {
            state.standby=a==POWER_STANDBY;state.playing=false;show(PLAYER);
        }
        return;
    }
    if(media_blocked() && (a==SHOW_VOLUME || a==SHOW_ROUTES || a==PLAY || a==PREVIOUS ||
       a==NEXT || a==MUTE || a==MINUS || a==PLUS || (a>=INPUT_BASE && a<OUTPUT_BASE+2)))return;
    if(ORBIT_LIVE && a!=SHOW_VOLUME && a!=SHOW_ROUTES && a!=BACK) {
        s3::Action action=s3::Action::Mute;int value=0;
        if(a>=INPUT_BASE && a<INPUT_BASE+5) {action=s3::Action::Input;value=a-INPUT_BASE;}
        else if(a>=OUTPUT_BASE && a<OUTPUT_BASE+2) {action=s3::Action::Output;value=a-OUTPUT_BASE;}
        else switch(a) {
            case PLAY:action=s3::Action::Toggle;break;
            case PREVIOUS:action=s3::Action::Previous;break;
            case NEXT:action=s3::Action::Next;break;
            case MUTE:value=!state.muted;break;
            case MINUS:volume(state.volume-1);return;
            case PLUS:volume(state.volume+1);return;
            default:return;
        }
        const bool sent=s3::send(action,value);
        statusFeedback.queued(action,value,remote,sent,lv_tick_get());render_status();
        if(sent && (action==s3::Action::Next || action==s3::Action::Previous)) {
            playbackProgress.beginTrackChange(lv_tick_get());render_progress();
        }
        return;
    }
    if(a>=INPUT_BASE&&a<INPUT_BASE+5) {
        int input=a-INPUT_BASE;
        if(input!=state.input) {state.input=input;if(input==0)state.network_active=false;}
    } else if(a>=OUTPUT_BASE&&a<OUTPUT_BASE+2) state.output=a-OUTPUT_BASE;
    else switch(a) {
        case SHOW_VOLUME:show(VOLUME);return;
        case SHOW_ROUTES:show(ROUTING);return;
        case BACK:show(PLAYER);return;
        case PLAY:if(playable())state.playing=!state.playing;break;
        case PREVIOUS:case NEXT:
            if(playable()) {state.track=(state.track+(a==NEXT?1:2))%3;state.elapsed=0;}break;
        case MUTE:state.muted=!state.muted;break;
        case MINUS:volume(state.volume-1);return;
        case PLUS:volume(state.volume+1);return;
    }
    render();
}
void render_progress() {
    if(!screen_awake || artwork_zoomed)return;
    const bool active=playable();
    const int total=ORBIT_LIVE?remote.duration:tracks[state.track].duration;
    const bool known=!ORBIT_LIVE || playbackProgress.known();
    const uint64_t positionMs=ORBIT_LIVE?playbackProgress.milliseconds(lv_tick_get()):uint64_t(state.elapsed)*1000;
    const int position=total>0?int(positionMs/1000):0;
    hidden(progress,!active || total<=0 || !known);hidden(elapsed,!active || total<=0 || !known);hidden(duration,!active || total<=0 || !known);
    lv_bar_set_value(progress,total>0?int(positionMs/uint64_t(total)):0,LV_ANIM_OFF); // range0..1000
    char value[32];snprintf(value,sizeof(value),"%d:%02d",position/60,position%60);text(elapsed,value);
    snprintf(value,sizeof(value),"%d:%02d",total/60,total%60);text(duration,value);
}
bool lyrics_available(){return lyrics_document && lyrics_document->count>=2 && playable() && !sleeping() && playbackProgress.known() && !playbackProgress.changing(lv_tick_get());}
void clear_lyrics(){
    if(lyrics_viewport){lv_anim_del(lyrics_viewport,lyrics_scroll);lyrics_scroll(nullptr,0);}
    orbit_lyrics::release(lyrics_document);lyrics_document=nullptr;lyrics_cursor=-999;
    if(lyrics_button)hidden(lyrics_button,true);
    if(state.view==LYRICS)show(PLAYER);
}
void lyrics_scroll(void *,int32_t value){if(!screen_awake||!lyrics_viewport)return;for(auto *row:lyrics_rows)if(row&&lv_obj_get_style_translate_y(row,0)!=value)lv_obj_set_style_translate_y(row,value,0);}

void render_lyrics(){
    if(!screen_awake||state.view!=LYRICS||slide_page)return;
    if(!lyrics_available()){show(PLAYER);return;}
    const int cursor=orbit_lyrics::current_line(*lyrics_document,playbackProgress.milliseconds(lv_tick_get()));
    if(cursor==lyrics_cursor&&lyrics_theme==theme_index)return;
    const int direction=lyrics_cursor>=0 && cursor>=0 && lyrics_theme==theme_index?(cursor==lyrics_cursor+1?1:cursor==lyrics_cursor-1?-1:0):0;
    const auto *neighbor=lyrics_rows[direction>0?3:1];
    const int previousY=direction?lv_obj_get_y(neighbor)+lv_obj_get_style_translate_y(neighbor,0):0;
    lv_anim_del(lyrics_viewport,lyrics_scroll);lyrics_scroll(nullptr,0);lyrics_cursor=cursor;lyrics_theme=theme_index;
    const int center=cursor<0?0:cursor;int heights[5];
    for(int i=0;i<5;++i){
        const int index=center+i-2;char normalized[orbit_lyrics::MaxLineBytes+1];
        const char *line=index>=0 && index<lyrics_document->count?lyrics_document->text+lyrics_document->lines[index].offset:"";
        orbit_metadata::compose_lyric(normalized,sizeof(normalized),line,strlen(line));text(lyrics_rows[i],normalized);
        lv_obj_set_style_text_font(lyrics_rows[i],&metadata_title_font,0);
        lv_obj_set_style_text_color(lyrics_rows[i],themed_color(FG),0);
        const lv_opa_t emphasis=i==2&&cursor>=0?LV_OPA_COVER:(i==0||i==4?96:160);
        lv_obj_set_style_text_opa(lyrics_rows[i],emphasis,0);
        lv_obj_update_layout(lyrics_rows[i]);heights[i]=lv_obj_get_height(lyrics_rows[i]);
        if(heights[i]>300){lv_obj_set_style_text_font(lyrics_rows[i],&metadata_artist_font,0);lv_obj_update_layout(lyrics_rows[i]);heights[i]=lv_obj_get_height(lyrics_rows[i]);}
        if(heights[i]<26)heights[i]=26;
    }
    int y[5];y[2]=(320-heights[2])/2;
    for(int i=1;i>=0;--i)y[i]=y[i+1]-14-heights[i];
    for(int i=3;i<5;++i)y[i]=y[i-1]+heights[i-1]+14;
    for(int i=0;i<5;++i)lv_obj_set_pos(lyrics_rows[i],20,y[i]);
    if(direction){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,lyrics_viewport);lv_anim_set_exec_cb(&a,lyrics_scroll);lv_anim_set_values(&a,previousY-y[2],0);lv_anim_set_time(&a,240);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);}
}
void render_clock() {
    if(!screen_awake || slide_page)return;
    char buffer[64];const auto &tools=orbit_time::state();const auto c=tools.config();
    if(state.view==CLOCK_ALERT){text(clock_alert_title,tools.alerts()==3?"ALRM + TMR":tools.alerts()&orbit_time::Alarm?"ALARM":"TIMER DONE");return;}
    if(state.view==ALARM_EDIT || state.view==TIMER_EDIT){
        const bool timer=state.view==TIMER_EDIT;unsigned first=timer?tools.remaining()/60:alarm_hour,second=timer?tools.remaining()%60:alarm_minute;
        auto **fields=timer?timer_fields:alarm_fields;snprintf(buffer,sizeof(buffer),"%02u",first);pixel_button_text(fields[0],buffer);snprintf(buffer,sizeof(buffer),"%02u",second);pixel_button_text(fields[1],buffer);
        const unsigned index=timer?1:0;for(unsigned i=0;i<2;++i)lv_obj_set_style_border_width(fields[i],edit_field[index]==i?3:0,LV_PART_MAIN);
        text(edit_selection[index],edit_field[index]?(timer?"SECOND":"MINUTE"):(timer?"MINUTE":"HOUR"));text(clock_save_hint[index],orbit_time::saved()?"":"NOT SAVED");
        if(timer){const bool running=tools.timer()==orbit_time::Timer::Running;pixel_button_text(timer_run,running?"PAUSE":tools.timer()==orbit_time::Timer::Paused?"RESUME":"START");for(auto *b:timer_fields)enabled(b,!running);enabled(edit_minus[1],!running);enabled(edit_plus[1],!running);}
        else {pixel_button_text(alarm_draft_toggle,alarm_enabled?"ON":"OFF");pixel_selected(alarm_draft_toggle,alarm_enabled);}
        return;
    }
    if(state.view!=BLANK_UP)return;
    clock_reading=orbit_clock::read();const bool ready=clock_reading.status==orbit_clock::Status::Ready && clock_reading.hour<24 && clock_reading.minute<60 && clock_reading.second<60 && clock_reading.day>=1 && clock_reading.day<=31 && clock_reading.month>=1 && clock_reading.month<=12 && clock_reading.weekday<7;
    if(ready){static const char *days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};static const char *months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};snprintf(buffer,sizeof(buffer),"%s %02u %s",days[clock_reading.weekday],unsigned(clock_reading.day),months[clock_reading.month-1]);text(clock_date,buffer);snprintf(buffer,sizeof(buffer),"%02u:%02u",unsigned(clock_reading.hour),unsigned(clock_reading.minute));text(clock_time,buffer);snprintf(buffer,sizeof(buffer),"%02u",unsigned(clock_reading.second));text(clock_seconds,buffer);text(clock_zone,clock_reading.zone);}else{text(clock_date,"");text(clock_time,"--:--");text(clock_seconds,"--");text(clock_zone,"");}
    text(clock_status,!orbit_time::saved()?"NOT SAVED":ready?"":clock_reading.status==orbit_clock::Status::Offline?"CONNECT TO WI-FI":clock_reading.status==orbit_clock::Status::Locating?"FINDING LOCATION":"SYNCING TIME");
    text(clock_alarm_caption,c.enabled?"NEXT ALARM":"ALARM OFF");
    auto *value=lv_obj_get_child(clock_alarm_value,0);lv_obj_set_style_text_font(value,c.enabled?&orbit_pixel_50:&orbit_pixel_21,0);
    if(c.enabled)snprintf(buffer,sizeof(buffer),"%02u:%02u",unsigned(c.hour),unsigned(c.minute));else strcpy(buffer,"SET ALARM");pixel_button_text(clock_alarm_value,buffer);
    pixel_button_text(clock_alarm_toggle,c.enabled?"ON":"OFF");pixel_selected(clock_alarm_toggle,c.enabled);
}
void draw_weather(lv_event_t *event){orbit_weather::draw_icon(event,weather_shape,lv_color_brightness(themed_color(BG))>160,weather_day);}
void render_weather() {
    if(!screen_awake || state.view!=BLANK_DOWN || slide_page)return;
    const auto value=orbit_weather::read();char buffer[128];
    text(weather_city,value.available?((snprintf(buffer,sizeof(buffer),"Near %s",value.city)),buffer):"Weather");
    hidden(weather_icon,!value.available);
    for(auto *caption:weather_metric_labels)hidden(caption,!value.available);
    if(!value.available){
        text(weather_temperature,"");text(weather_range,"");text(weather_details,"");text(weather_wind,"");text(weather_humidity,"");text(weather_precip,"");text(weather_age,"");
        const char *message=value.status==orbit_weather::Status::Offline?"Connect to Wi-Fi":value.status==orbit_weather::Status::Syncing?"Syncing time":value.status==orbit_weather::Status::Locating?"Finding your area":value.status==orbit_weather::Status::Loading?"Updating weather":"Weather unavailable";
        text(weather_condition,message);return;
    }
    const auto shape=orbit_weather::icon(value.data.code,value.data.day);
    if(shape!=weather_shape || weather_day!=value.data.day){weather_shape=shape;weather_day=value.data.day;lv_obj_invalidate(weather_icon);}
    snprintf(buffer,sizeof(buffer),"%d°C",int(lroundf(value.data.temperature)));text(weather_temperature,buffer);
    text(weather_condition,orbit_weather::description(value.data.code,value.data.day));
    snprintf(buffer,sizeof(buffer),"H %d°   /   L %d°",int(lroundf(value.data.high)),int(lroundf(value.data.low)));text(weather_range,buffer);
    snprintf(buffer,sizeof(buffer),"Feels like %d°C",int(lroundf(value.data.feels)));text(weather_details,buffer);
    snprintf(buffer,sizeof(buffer),"%.0f",double(value.data.wind));text(weather_wind,buffer);
    snprintf(buffer,sizeof(buffer),"%u%%",unsigned(value.data.humidity));text(weather_humidity,buffer);
    snprintf(buffer,sizeof(buffer),"%u%%",unsigned(value.data.rain));text(weather_precip,buffer);
    snprintf(buffer,sizeof(buffer),value.status==orbit_weather::Status::Saved?"Saved %um ago":"Updated %um ago",unsigned(value.ageMinutes));text(weather_age,buffer);
}
void render_iphone() {
    using L=orbit_iphone::LaunchState;
    const auto value=orbit_iphone::state();const char *message="";
    switch(value){
        case L::Unconfigured:message="Not set up";break;
        case L::Offline:message="Wi-Fi unavailable";break;
        case L::Syncing:message="Connecting";break;
        case L::Sending:message="Sending";break;
        case L::Sent:message="Request sent";break;
        case L::Uncertain:message="Check iPhone";break;
        case L::Failed:message="Could not send";break;
        case L::Cooldown:message="Please wait";break;
        case L::Ready:break;
    }
    if(iphone_waking)message="Waking S3";
    else if(iphone_error && uint32_t(lv_tick_get()-iphone_error_at)<4000)message=iphone_error;
    else if(value==L::Ready && !iphone_s3_available())message=power_busy()?"Please wait":"S3 unavailable";
    enabled(iphone_button,value==L::Ready && !iphone_waking && iphone_s3_available());
    text(iphone_status,message);
}
void render() {
    observe_intro_track(); // cache lifetime continues while the display sleeps
    if(!screen_awake)return;
    if(lyrics_button)hidden(lyrics_button,!lyrics_available() || state.view!=PLAYER);
    if(state.view==LYRICS){render_lyrics();return;}
    if(state.view==INTRO || intro_reader(state.view)){render_intro();return;}
    if(state.view==BLANK_UP || state.view==ALARM_EDIT || state.view==TIMER_EDIT || state.view==CLOCK_ALERT){render_clock();return;}
    if(state.view==BLANK_DOWN){render_weather();return;}
    if(state.view==BLANK_LEFT){render_iphone();return;}
    if(blank_view(state.view))return;
    if(artwork_zoomed) {
        const bool same=ORBIT_LIVE?artwork_view_revision==remote.artworkRevision:artwork_view_track==state.track;
        if(has_cover() && same)return;
        drop_artwork_view();show(PLAYER);return;
    }
    if(state.view==ABOUT) {
        const auto feedback=statusFeedback.current(remote,lv_tick_get(),ORBIT_LIVE);
        text(about_status,feedback==orbit_status::Icon::Error?"Unable to update":
             feedback==orbit_status::Icon::Limited?"Some controls unavailable":"");
        text(about_values[0],ORBIT_LIVE?(remote.localIp[0]?remote.localIp:"Not connected"):"Not connected");
        text(about_values[1],ORBIT_LIVE?(remote.deviceIp[0]?remote.deviceIp:"Not found"):"Not connected");
        text(about_values[2],!ORBIT_LIVE?"Demo":remote.connected?"Connected":!remote.wifiConnected?"Wi-Fi offline":remote.deviceIp[0]?(remote.link==s3::LinkState::Waiting?"Reconnecting":"S3 offline"):"Searching");
        text(about_values[4],!ORBIT_LIVE || !remote.wifiConnected?"Not connected":
             remote.wifiRssi>=-60?"Strong":remote.wifiRssi>=-75?"Good":"Weak");
        text(about_values[5],ORBIT_LIVE?s3::bluetooth_status_label(remote.bluetooth):"Unknown");
        text(about_input_key,ORBIT_LIVE?s3::input_status_key(remote):"Input");
        text(about_values[6],ORBIT_LIVE?s3::input_status_value(remote):inputs[state.input]);
        text(about_values[7],ORBIT_FIRMWARE_VERSION);
        text(about_values[3],ORBIT_LIVE?(remote.connected?s3::power_label(remote.power):"Unknown"):state.standby?"Standby":"Online");
    }
    lv_arc_set_range(ring,0,volume_max()>0?volume_max():1);
    lv_arc_set_value(ring,state.volume);
    lv_obj_set_style_arc_color(ring,themed_color(state.muted?MUTED:0x9bcead),LV_PART_INDICATOR);
    if(ORBIT_LIVE && !remote.volumeKnown) {
        text(chip_text,"--");text(number,"--");
    } else {
        if(state.muted)text(chip_text,LV_SYMBOL_MUTE"  MUTED");
        else lv_label_set_text_fmt(chip_text,LV_SYMBOL_VOLUME_MID"  %d",state.volume);
        lv_label_set_text_fmt(number,"%d",state.volume);
    }
    const char *out=state.output==0?"ANALOG":state.output==1?"OPTICAL":"UNKNOWN";
    if(ORBIT_LIVE && (!remote.connected || remote.volumeMax<=0)) text(volume_unit,remote.connected?"VOLUME UNAVAILABLE":"S3 OFFLINE");
    else if(ORBIT_LIVE && remote.volumeCeiling<0) text(volume_unit,"READ ONLY");
    else if(volume_limit_feedback)lv_label_set_text_fmt(volume_unit,"LIMIT %d",volume_cap());
    else if(!remote.volumeLimitKnown && ORBIT_LIVE)text(volume_unit,"LIMIT UNAVAILABLE");
    else if(pending_volume) text(volume_unit,"UPDATING");
    else if(state.muted) text(volume_unit,"MUTED");
    else lv_label_set_text_fmt(volume_unit,"%s OUT",out);
    lv_obj_set_style_text_color(chip_text,themed_color(state.muted?MUTED:GREEN),0);
    text(mute_text,state.muted?"Unmute":"Mute");
    lv_obj_set_style_text_font(mute_text,state.muted?&lv_font_montserrat_12:&lv_font_montserrat_14,0);
    text(lv_obj_get_child(mute_buttons[0],0),state.muted?LV_SYMBOL_MUTE:LV_SYMBOL_VOLUME_MID);
    hidden(mute_slash,!state.muted);
    lv_obj_set_style_line_color(mute_slash,themed_color(MUTED),0);
    // The home action stays an unframed icon. Its glyph reports confirmed state.
    lv_obj_set_style_text_color(lv_obj_get_child(mute_buttons[0],0),themed_color(state.muted?MUTED:FG),0);
    // The volume page keeps three matching round controls; no inverse highlight.
    lv_obj_set_style_bg_color(mute_buttons[1],themed_color(0x17221d),0);
    lv_obj_set_style_text_color(mute_text,themed_color(FG),0);
    text(volume_hint,volume_limit_feedback?"Volume limit reached":"");
    const char *src=state.input>=0 && state.input<5?inputs[state.input]:"Unknown source";
    s3::SourceIcon icon=s3::physical_source_icon(state.input);
    if(state.input==0) {
        const auto identity=s3::identify_protocol(ORBIT_LIVE?remote.protocol:(state.network_active?"spotify":""));
        src=identity.label;icon=identity.icon;
        if(!ORBIT_LIVE && !state.network_active) src="Network";
    }
    char caption[64]={};
    for(size_t i=0;src[i] && i<sizeof(caption)-1;++i) caption[i]=char(toupper(static_cast<unsigned char>(src[i])));
    text(source,caption);text(route_source,src);
    lv_coord_t captionWidth=lv_txt_get_width(caption,strlen(caption),&lv_font_montserrat_12,0,LV_TEXT_FLAG_NONE);
    lv_obj_set_width(source,captionWidth>250?250:captionWidth);
    source_image(source_icon,icon);source_image(route_source_icon,icon);
    const int route_w=lv_txt_get_width(src,strlen(src),&lv_font_montserrat_14,0,LV_TEXT_FLAG_NONE);
    const int route_text_w=route_w>204?204:route_w;
    const int icon_space=s3::source_icon_image(icon)?20+orbit_layout::Gap:0;
    const int route_left=orbit_layout::centered(256,icon_space+route_text_w);
    lv_obj_set_pos(route_source_icon,route_left,7);
    lv_obj_set_pos(route_source,route_left+icon_space,orbit_layout::centered(34,lv_font_montserrat_14.line_height));
    lv_obj_set_width(route_source,route_text_w);
    lv_obj_set_style_text_align(route_source,LV_TEXT_ALIGN_LEFT,0);
    const Track &t=tracks[state.track];const bool active=playable();
    render_metadata(t,active);
    hidden(intro_entry,!intro_available());render_intro();
    const bool has_art=has_cover();
    hidden(cover,!has_art);
    vinyl_visible=!has_art;
    hidden(vinyl,!vinyl_visible || sleeping());
    hidden(vinyl_shine,!vinyl_visible || !vinyl_pixels || sleeping());hidden(tonearm,!vinyl_visible || sleeping());
    set_arm_target();
    lv_obj_set_style_img_opa(cover,LV_OPA_COVER,0);
    enabled(transport[0],active && (!ORBIT_LIVE || remote.canPrevious));
    enabled(transport[1],active && (!ORBIT_LIVE || remote.canToggle));
    enabled(transport[2],active && (!ORBIT_LIVE || remote.canNext));
    for(auto *b:volume_buttons) {
        const bool available=volume_available();enabled(b,available);
        // Disabled must remain legible, with a visible button outline.
        if(!available)lv_obj_set_style_opa(b,LV_OPA_70,0);
        for(uint32_t i=1;i<lv_obj_get_child_cnt(b);++i)
            lv_obj_set_style_line_color(lv_obj_get_child(b,i),themed_color(FG),0);
    }
    for(auto *b:mute_buttons) enabled(b,!media_blocked() && (!ORBIT_LIVE || (remote.connected && remote.muteKnown)));
    text(play_text,active && state.playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);
    render_progress();
    for(int i=1;i<5;i++) {selected(input_btns[i],i==state.input);enabled(input_btns[i],!media_blocked() && (!ORBIT_LIVE || (remote.connected && remote.sourceKnown)));}
    for(int i=0;i<2;i++) {selected(output_btns[i],i==state.output);enabled(output_btns[i],!media_blocked() && (!ORBIT_LIVE || (remote.connected && remote.outputKnown)));}
    const bool asleep=sleeping();
    hidden(wake_button,!asleep);
    enabled(wake_button,asleep && !power_busy());
    enabled(standby_button,!asleep && !power_busy() && (!ORBIT_LIVE ||
        (remote.connected && remote.power==s3::PowerState::Online)));
    hidden(home_io,asleep);hidden(mute_buttons[0],asleep);
    enabled(home_io,!media_blocked());enabled(chip,!media_blocked());
    hidden(title,asleep);hidden(artist,asleep);
    for(auto *b:transport)hidden(b,asleep);
    const bool overlay=state.view!=PLAYER && state.view!=VOLUME;
    hidden(chip,overlay || asleep);hidden(source_row,overlay || asleep);hidden(ring,overlay || asleep);
    render_status();
}
void tick(lv_timer_t *) {
    const uint32_t now=lv_tick_get();
#if ORBIT_INTRO_ASYNC
    if(intro_client){
        const bool changed=intro_client->tick(orbit_intro_runtime::nowMs());const auto outcome=intro_client->outcome();
        const bool waiting=outcome==orbit_intro_ai::Outcome::Pending && now/150!=intro_frame;
        const uint32_t wait=uint32_t((uint64_t(intro_client->retryMs(orbit_intro_runtime::nowMs()))+999)/1000);
        const bool cooldown=orbit_intro_async::retryable(outcome) && wait!=intro_wait;
        if(screen_awake && !slide_page && state.view==INTRO_READ && (changed||waiting||cooldown))render_intro();
    }
#endif
    if(uint32_t(now-clock_poll_at)>=1000){clock_poll_at=now;alarm_wall=orbit_clock::trusted_read();}
    // Recheck only a due candidate; never ring from the1s cached old zone/time.
    if(orbit_time::state().due(alarm_wall))alarm_wall=orbit_clock::trusted_read();
    orbit_time::tick(now,alarm_wall);
    if(orbit_time::state().alerts() && state.view!=CLOCK_ALERT){consume_swipe_contact();show(CLOCK_ALERT);}

    static unsigned lastTimerSeconds=~0u;const unsigned timerSeconds=orbit_time::state().remaining();
    if(screen_awake && !slide_page && state.view==TIMER_EDIT && timerSeconds!=lastTimerSeconds){lastTimerSeconds=timerSeconds;render_clock();}
    if(screen_awake && state.view==LYRICS)render_lyrics();
    update_iphone_wake(false);
    if(!slide_page)animate_tonearm();render_status();
    if(screen_awake && !slide_page && state.view==BLANK_LEFT)render_iphone();
    if(volume_limit_feedback && int32_t(lv_tick_get()-volume_limit_until)>=0) {volume_limit_feedback=false;render();}
    static uint32_t lastClock=0;
    if(screen_awake && (state.view==BLANK_UP || state.view==TIMER_EDIT || state.view==ALARM_EDIT || state.view==CLOCK_ALERT) && uint32_t(lv_tick_get()-lastClock)>=1000) {
        lastClock=lv_tick_get();render_clock();
    }
    static uint32_t lastWeather=0;
    if(screen_awake && state.view==BLANK_DOWN && uint32_t(lv_tick_get()-lastWeather)>=1000){lastWeather=lv_tick_get();render_weather();}
    static uint32_t lastAbout=0;
    if(state.view==ABOUT && uint32_t(lv_tick_get()-lastAbout)>=1000) {lastAbout=lv_tick_get();render();}
    if(ORBIT_LIVE) {
        s3::Snapshot update;
        if(s3::receive(update)) {
            const bool enteredStandby=update.connected && update.power==s3::PowerState::Standby &&
                (!remote.connected || remote.power!=s3::PowerState::Standby);
            if(strcmp(remote.title,update.title) || strcmp(remote.artist,update.artist))metadata_dirty=true;
            playbackProgress.observe(remote,update,lv_tick_get());remote=update;
            const uint32_t lyricGeneration=orbit_lyrics::observe(remote);
            if(lyricGeneration!=lyrics_generation){lyrics_generation=lyricGeneration;clear_lyrics();}
            // A disconnected/unverifiable drag must not resume with accumulated
            // pointer movement after reconnection; require a new press.
            if(!volume_available()) {dragging=false;contact_can_write=false;gesture_volume=remote.volume;}
            if(s3::power_blocks_media(remote)) {pending_volume=false;volume_limit_feedback=false;}
            statusFeedback.observe(update,lv_tick_get());
            update_iphone_wake(true);
            if(live_cover_ready && (live_artwork.revision!=remote.artworkRevision || strcmp(live_artwork.url,remote.artwork)!=0)) {
                if(artwork_zoomed){drop_artwork_view();show(PLAYER);}
                lv_img_cache_invalidate_src(&live_cover_image);
                lv_img_set_src(cover,LV_SYMBOL_AUDIO);
                free(live_artwork.data);free(live_artwork.large);live_artwork={};live_cover_ready=false;
            }
            if(pending_volume && (remote.volume==state.volume || !remote.connected || uint32_t(lv_tick_get()-pending_volume_at)>5000)) pending_volume=false;
            if(!dragging && !pending_volume) state.volume=remote.volume;
            // Retain the last input as context during a transient read/reconnect;
            // every actual control still checks fresh remote.sourceKnown/online.
            if(remote.sourceKnown)state.input=remote.input;
            if(pending_volume && remote.volumeLimitKnown && state.volume>volume_cap()) {
                pending_volume=false;state.volume=remote.volume;
                // A newly lowered app limit legitimately supersedes the old target.
                // Cancel waiting without showing a false check or timeout error.
                if(statusFeedback.pending && statusFeedback.action==s3::Action::Volume)
                    statusFeedback.finish(orbit_status::Icon::Connected,lv_tick_get(),0);
                volume_limit_feedback=true;volume_limit_until=lv_tick_get()+1600;
            }
            state.output=remote.outputKnown?remote.output:-1;
            state.muted=remote.muted;state.playing=remote.playing;
            // S3 power changes only redirect its own control subpages. Local pages
            // and an active spatial transition retain their navigation destination.
            if(enteredStandby && !blank_view(state.view) && !slide_page)show(PLAYER);else render();
        }
        orbit_lyrics::Result lyricResult;
        if(orbit_lyrics::receive(lyricResult)){
            if(lyricResult.generation==lyrics_generation){clear_lyrics();lyrics_document=lyricResult.document;lyricResult.document=nullptr;render();}
            orbit_lyrics::release(lyricResult.document);
        }
        s3::Artwork art;
        if(s3::receiveArtwork(art)) {
            if(art.revision==remote.artworkRevision && strcmp(art.url,remote.artwork)==0 && art.data && art.size==s3::ArtworkBytes && art.large && art.largeSize==s3::ArtworkLargeBytes) {
                // Detach the viewer before replacing either cache, even for the same revision.
                if(artwork_zoomed){drop_artwork_view();show(PLAYER);}
                lv_img_cache_invalidate_src(&live_cover_image);
                lv_img_set_src(cover,LV_SYMBOL_AUDIO);
                free(live_artwork.data);free(live_artwork.large);live_artwork=art;
                live_cover_image={};live_cover_image.header.always_zero=0;
                live_cover_image.header.cf=LV_IMG_CF_TRUE_COLOR;
                live_cover_image.header.w=170;live_cover_image.header.h=170;
                live_cover_image.data=art.data;live_cover_image.data_size=art.size;
                lv_img_set_src(cover,&live_cover_image);
                live_cover_ready=true;art.data=art.large=nullptr;
            }
            free(art.data);free(art.large);render();
        }
        // Only the small progress widgets redraw between device snapshots.
        if(state.view==PLAYER)render_progress();
        return;
    }
    static uint32_t last=0;
    if(uint32_t(lv_tick_get()-last)<1000) return;
    last=lv_tick_get();
    if(!playable()||!state.playing) return;
    if(++state.elapsed>=tracks[state.track].duration) {state.elapsed=0;state.track=(state.track+1)%3;}
    render();
}
View swipe_destination(View origin,orbit_touch::SwipeDirection d) {
    using D=orbit_touch::SwipeDirection;
    // Finger left reveals the page to the right; reverse direction returns home.
    if(origin==INTRO && d==D::Right)return PLAYER;
    if(origin==INTRO_SOURCES && d==D::Right)return INTRO_READ;
    if(origin==INTRO_READ && d==D::Right)return INTRO;
    if(origin==PLAYER){
        if(d==D::Left)return BLANK_RIGHT;if(d==D::Right)return BLANK_LEFT;
        if(d==D::Up)return BLANK_DOWN;if(d==D::Down)return BLANK_UP;
    }
    if((origin==BLANK_RIGHT && d==D::Right)||(origin==BLANK_LEFT && d==D::Left)||
       (origin==BLANK_DOWN && d==D::Down)||((origin==BLANK_UP || origin==ALARM_EDIT || origin==TIMER_EDIT) && d==D::Up))return PLAYER;
    return origin;
}
bool player_control_at(const lv_point_t &point) {
    // Disabled buttons are skipped by LVGL's hit search. Keep their visible
    // targets distinct from the volume arc; clear swipes may start on them too.
    lv_obj_t *controls[]={chip,home_io,mute_buttons[0],transport[0],transport[1],transport[2],status_target,wake_button,lyrics_button,intro_entry};
    for(auto *object:controls) {
        bool visible=true;for(auto *o=object;o;o=lv_obj_get_parent(o))if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){visible=false;break;}
        if(!object || !visible)continue;
        lv_area_t area;lv_obj_get_click_area(object,&area);if(!_lv_area_is_point_on(&area,&point,0))continue;
        if(lv_obj_has_flag(object,LV_OBJ_FLAG_ADV_HITTEST)){
            lv_hit_test_info_t hit={&point,true};lv_event_send(object,LV_EVENT_HIT_TEST,&hit);if(!hit.res)continue;
        }
        return true;
    }
    return false;
}
// Resolve geometry only after contact intent is known. Button geometry keeps
// the volume arc from owning an overlapping button TAP. Drags use the arc geometry
// directly, so expanded button targets cannot steal an outer-ring volume gesture.
bool volume_contact(lv_point_t point) {
    return (swipe_origin==PLAYER || swipe_origin==VOLUME) &&
        orbit_touch::volume_track(point.x,point.y) &&
        !(swipe_origin==PLAYER && player_control_at(point)) &&
        lv_indev_search_obj(lv_scr_act(),&point)==root;
}
void consume_swipe_contact() {
    if(auto *input=lv_indev_get_act())lv_indev_wait_release(input);
}
} // namespace

void orbit_ui_touch_sample(int16_t x,int16_t y,bool pressed) {
    if(!root || !screen_awake){swipe.reset();dragging=false;return;}
    // If a new finger arrives mid-transition, consume its entire contact even
    // when the animation finishes while it is still held. Never revive a tap.
    if(slide_page || slide_contact_blocked) {
        if(pressed)slide_contact_blocked=true;
        consume_swipe_contact();swipe.reset();dragging=false;
        if(!pressed)slide_contact_blocked=false;
        return;
    }
    const uint32_t now=lv_tick_get();
    if(pressed && !swipe.active()) {
        // Pending contact: no region hit-test, navigation or audio operation.
        swipe.begin(x,y,now);contact_start={x,y};contact_owner=ContactOwner::Pending;
        swipe_origin=state.view;swipe_zoomed=artwork_zoomed;swipe_sleeping=sleeping();
        contact_can_write=volume_available();
        return;
    }
    if(!swipe.active())return;
    const bool own_volume=contact_owner==ContactOwner::Volume && state.view==VOLUME;
    const bool changed=(state.view!=swipe_origin && !own_volume) || artwork_zoomed!=swipe_zoomed || sleeping()!=swipe_sleeping;
    if(changed)swipe.cancel();
    swipe.move(x,y);
    if(swipe.cancelled()) {
        contact_owner=ContactOwner::Blocked;dragging=false;consume_swipe_contact();
    } else if(swipe.moved()) {
        // Drag intent wins over every button tap, including disabled controls.
        // Reassert on every sample so LVGL input resets cannot revive a click.
        if(contact_owner==ContactOwner::Pending && swipe_origin==INTRO_READ &&
           intro_read_contact(contact_start) && abs(y-contact_start.y)>abs(x-contact_start.x)*6/5)
            contact_owner=ContactOwner::Reading;
        if(contact_owner!=ContactOwner::Reading)consume_swipe_contact();
        if(contact_owner==ContactOwner::Pending) {
            contact_owner=ContactOwner::Blocked;
            const int dx=contact_start.x-240,dy=contact_start.y-240;
            if(!artwork_zoomed && dx*dx+dy*dy<=240*240) {
                if(!sleeping() && !power_busy() && (swipe_origin==PLAYER || swipe_origin==VOLUME) &&
                   orbit_touch::volume_track(contact_start.x,contact_start.y)) {
                    contact_owner=ContactOwner::Volume;
                    last_angle=angle(contact_start);gesture_volume=state.volume;
                    dragging=contact_can_write && volume_available();show(VOLUME);
                } else if(swipe_origin==PLAYER || blank_view(swipe_origin) || swipe_origin==INTRO || intro_reader(swipe_origin))contact_owner=ContactOwner::Page;
            }
        }
        if(contact_owner==ContactOwner::Volume && pressed && dragging) {
            // Crossing the bottom gap or losing permission ends the whole drag.
            if(!orbit_touch::track_angle(x,y) || !volume_available())dragging=false;
            else {
                const float a=angle({x,y});float d=a-last_angle;
                if(d>180)d-=360;if(d< -180)d+=360;
                gesture_volume+=d*volume_max()/270.0f;
                if(gesture_volume<0)gesture_volume=0;if(gesture_volume>volume_max())gesture_volume=volume_max();
                last_angle=a;volume(static_cast<int>(roundf(gesture_volume)));
            }
        }
    }
    // Vertical reading belongs to LVGL's native scroll/momentum engine. Do not
    // wait-release it: that would cancel the contact before LVGL can follow it.
    if(contact_owner==ContactOwner::Reading){if(!pressed)swipe.end(x,y,now);dragging=false;return;}
    if(pressed)return;
    const bool tap=!swipe.moved() && !swipe.cancelled();
    const auto direction=swipe.end(x,y,now);
    if(tap && !changed && !artwork_zoomed && !sleeping() && !power_busy() && volume_contact(contact_start)) {
        // A ring tap opens Volume only on release, without changing its value.
        consume_swipe_contact();show(VOLUME);
    } else if(contact_owner==ContactOwner::Page && !changed) {
        const View next=swipe_destination(swipe_origin,direction);
        if(next!=state.view){consume_swipe_contact();show_swipe_page(next,direction);}
    }
    dragging=false;
}

bool orbit_ui_alert_active(){return orbit_time::state().alerts()!=0;}
bool orbit_ui_keep_screen_on(){return orbit_ui_alert_active() || (screen_awake && state.view==LYRICS);}

bool orbit_ui_record_motion_active() {
    return screen_awake && !slide_page && !artwork_zoomed && ORBIT_ANIMATION_INTERVAL_MS>0 && vinyl_pixels && vinyl_visible && state.view==PLAYER && playable() && state.playing;
}

void orbit_ui_set_screen_awake(bool awake) {
    if(screen_awake==awake)return;
    screen_awake=awake;intro_scrolling();
    if(!awake) {
        stop_intro_motion();
        lv_anim_del(lyrics_viewport,lyrics_scroll);lyrics_cursor=-999;
        const bool transitioning=slide_page!=nullptr;const View target=slide_target;
        stop_page_slide();if(transitioning && state.view!=target)show(target);
        dragging=false;swipe.reset();
        if(artwork_zoomed){lv_anim_del(artwork_large,artwork_frame);artwork_frame(nullptr,artwork_target);artwork_finished(nullptr);}
        return;
    }
    render();render_progress();
}

void orbit_ui_create() {
    orbit_time::begin();
    // UI controls stay English; UTF-8 metadata keeps its original text.
    // Use Flash-resident CJK glyphs only when Montserrat has no matching glyph.
    metadata_title_top=orbit_metadata_prepare_font(&metadata_title_font,false);
    metadata_artist_top=orbit_metadata_prepare_font(&metadata_artist_font,true);
    // This fixed dashboard has no scrollable screen. Otherwise LVGL can pass a
    // read-only ring drag to the default parent screen and move the whole UI.
    lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
    root=box(lv_scr_act(),0,0,480,480,BG);
    // Fill all480x480 RGB pixels; the physical panel supplies the circular edge.
    // A rounded root can expose the default light screen beneath its AA fringe.
    lv_obj_set_style_radius(root,0,0);
    // The panel clips at its physical circular edge; content is laid out inside
    // that edge. Avoid a redundant full-screen mask around every child draw.
    lv_obj_set_style_clip_corner(root,false,0);
    lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    ring=lv_arc_create(root);lv_obj_remove_style_all(ring);
    lv_obj_set_pos(ring,15,15);lv_obj_set_size(ring,450,450);
    lv_arc_set_rotation(ring,135);lv_arc_set_bg_angles(ring,0,270);
    lv_arc_set_range(ring,0,100);
    lv_obj_set_style_arc_width(ring,3,LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring,themed_color(TRACK),LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(ring,true,LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring,4,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring,true,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ring,themed_color(GREEN),LV_PART_KNOB);
    lv_obj_set_style_bg_opa(ring,LV_OPA_COVER,LV_PART_KNOB);
    lv_obj_set_style_pad_all(ring,4,LV_PART_KNOB);
    lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,LV_PART_KNOB);
    // Relative dragging is handled by the unified contact arbiter; no absolute jump.
    lv_obj_clear_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    for(auto &page:pages) {page=box(root,0,0,480,480,BG);lv_obj_set_style_bg_opa(page,LV_OPA_TRANSP,0);}
    auto *lyricPage=pages[LYRICS];lv_obj_add_flag(lyricPage,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(lyricPage,click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(CLOSE_LYRICS)));
    lyrics_viewport=box(lyricPage,50,80,380,320,BG);lv_obj_set_style_bg_opa(lyrics_viewport,LV_OPA_TRANSP,0);
    for(auto &row:lyrics_rows){row=label(lyrics_viewport,"",20,0,340,&metadata_title_font,MUTED);lv_label_set_long_mode(row,LV_LABEL_LONG_WRAP);}
    auto *intro=pages[INTRO];lv_obj_add_event_cb(intro,draw_intro_orbit,LV_EVENT_DRAW_MAIN,nullptr);
    intro_subject=metadata_label(intro,132,212,216,&metadata_title_font,FG);
    intro_context=metadata_label(intro,140,250,200,&metadata_artist_font);
    const int positions[][2]={{184,70},{48,300},{320,300}};
    for(unsigned i=0;i<3;++i){
        auto *b=button(intro,intro_names[i],positions[i][0],positions[i][1],112,96,INTRO_TOPIC_BASE+i);intro_choices[i]=b;lv_obj_set_ext_click_area(b,4);lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,intro_radial_hit,LV_EVENT_HIT_TEST,nullptr);
        lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,LV_STATE_PRESSED);
        auto *name=lv_obj_get_child(b,0);lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(name,0,54);lv_obj_set_width(name,112);lv_obj_set_style_text_font(name,&lv_font_montserrat_16,0);
        auto *symbol=lv_img_create(b);lv_img_set_src(symbol,orbit_intro::icon(i));lv_obj_set_pos(symbol,40,12);lv_obj_set_style_img_recolor(symbol,themed_color(GREEN),0);lv_obj_set_style_img_recolor_opa(symbol,LV_OPA_COVER,0);lv_obj_clear_flag(symbol,LV_OBJ_FLAG_CLICKABLE);
    }
    back_button(intro,164,412,52,INTRO_BACK);
    auto *reader=pages[INTRO_READ];
    intro_read_kind=label(reader,"",80,62,320,&lv_font_montserrat_14,MUTED);
    intro_read_subject=metadata_label(reader,70,94,340,&metadata_title_font,FG);
    box(reader,100,135,280,1,TRACK);
    intro_viewport=box(reader,76,152,328,236,BG);lv_obj_set_style_bg_opa(intro_viewport,LV_OPA_TRANSP,0);
    lv_obj_add_flag(intro_viewport,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(intro_viewport,LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_set_scroll_dir(intro_viewport,LV_DIR_VER);lv_obj_set_scrollbar_mode(intro_viewport,LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_width(intro_viewport,2,LV_PART_SCROLLBAR);lv_obj_set_style_bg_color(intro_viewport,themed_color(MUTED),LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(intro_viewport,LV_OPA_70,LV_PART_SCROLLBAR);
    intro_body=label(intro_viewport,"",12,0,304,&metadata_title_font,FG);lv_obj_set_style_text_align(intro_body,LV_TEXT_ALIGN_LEFT,0);lv_obj_set_style_text_line_space(intro_body,8,0);lv_label_set_long_mode(intro_body,LV_LABEL_LONG_WRAP);
#if ORBIT_INTRO_ASYNC
    intro_paragraph2=label(intro_viewport,"",12,0,304,&metadata_title_font,FG);lv_obj_set_style_text_align(intro_paragraph2,LV_TEXT_ALIGN_LEFT,0);lv_obj_set_style_text_line_space(intro_paragraph2,8,0);lv_label_set_long_mode(intro_paragraph2,LV_LABEL_LONG_WRAP);
    intro_source_heading=label(intro_viewport,"Source",12,0,304,&lv_font_montserrat_16,MUTED);lv_obj_set_style_text_align(intro_source_heading,LV_TEXT_ALIGN_LEFT,0);hidden(intro_source_heading,true);
    lv_obj_set_style_pad_bottom(intro_viewport,12,0);
    for(unsigned i=0;i<4;++i){
        char number[8];snprintf(number,sizeof number,"[%u]",i+1);
        auto *b=button(intro_viewport,number,12,0,304,60,INTRO_SOURCE_BASE+i);intro_references[i]=b;
        lv_obj_set_ext_click_area(b,0);lv_obj_set_style_radius(b,0,0);lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,LV_STATE_PRESSED);
        lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,intro_reference_hit,LV_EVENT_HIT_TEST,nullptr);
        auto *index=lv_obj_get_child(b,0);lv_obj_set_align(index,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(index,0,8);lv_obj_set_width(index,30);lv_obj_set_style_text_font(index,&metadata_artist_font,0);lv_obj_set_style_text_align(index,LV_TEXT_ALIGN_LEFT,0);lv_obj_set_style_text_color(index,themed_color(MUTED),0);
        intro_reference_titles[i]=label(b,"",36,8,268,&metadata_artist_font,FG);lv_obj_set_style_text_align(intro_reference_titles[i],LV_TEXT_ALIGN_LEFT,0);lv_obj_set_style_text_line_space(intro_reference_titles[i],3,0);lv_label_set_long_mode(intro_reference_titles[i],LV_LABEL_LONG_WRAP);hidden(b,true);
    }
    auto *sources=pages[INTRO_SOURCES];
    intro_source_title=label(sources,"",96,52,288,&metadata_artist_font,FG);lv_label_set_long_mode(intro_source_title,LV_LABEL_LONG_DOT);lv_obj_set_height(intro_source_title,48);
    intro_source_domain=label(sources,"",90,108,300,&lv_font_montserrat_14,MUTED);
    intro_qr=box(sources,140,138,200,200,BG);lv_obj_set_style_bg_opa(intro_qr,LV_OPA_TRANSP,0);lv_obj_add_event_cb(intro_qr,draw_intro_qr,LV_EVENT_DRAW_MAIN,nullptr);
    intro_source_error=label(sources,"",100,150,280,&lv_font_montserrat_14,FG);lv_label_set_long_mode(intro_source_error,LV_LABEL_LONG_DOT);lv_obj_set_height(intro_source_error,188);hidden(intro_source_error,true);
    for(unsigned i=0;i<4;++i){char number[8];snprintf(number,sizeof number,"[%u]",i+1);auto *b=button(sources,number,120+60*i,338,60,60,INTRO_SOURCE_BASE+i);intro_source_tabs[i]=b;lv_obj_set_ext_click_area(b,0);lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0);lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_montserrat_16,0);}
    back_button(sources,164,412,52,INTRO_SOURCE_BACK);
    intro_loading=lv_img_create(reader);lv_obj_set_pos(intro_loading,228,207);lv_obj_set_style_img_recolor_opa(intro_loading,LV_OPA_COVER,0);hidden(intro_loading,true);
    intro_message=label(reader,"",88,250,304,&lv_font_montserrat_18,FG);lv_label_set_long_mode(intro_message,LV_LABEL_LONG_WRAP);
    intro_retry=button(reader,"Retry",168,318,144,52,INTRO_RETRY);lv_obj_set_style_radius(intro_retry,26,0);lv_obj_set_style_bg_opa(intro_retry,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(intro_retry,1,0);lv_obj_set_style_border_color(intro_retry,themed_color(TRACK),0);lv_obj_set_style_text_font(lv_obj_get_child(intro_retry,0),&lv_font_montserrat_18,0);hidden(intro_retry,true);
    intro_feedback=label(reader,"Local sample",90,391,300,&lv_font_montserrat_12,MUTED);
#endif
    back_button(reader,164,412,52,INTRO_BACK);
    // Dedicated monochrome pixel clock. Only meaningful, tappable information.
    for(auto v:{BLANK_UP,ALARM_EDIT,TIMER_EDIT,CLOCK_ALERT}){lv_obj_set_style_bg_color(pages[v],lv_color_hex(0),0);lv_obj_set_style_bg_opa(pages[v],LV_OPA_COVER,0);}
    auto *clock=pages[BLANK_UP];
    clock_date=pixel_label(clock,"",80,68,320);
    clock_time=pixel_label(clock,"--:--",55,125,370,&orbit_pixel_100);
    clock_seconds=pixel_label(clock,"--",286,237,64,&orbit_pixel_40);pixel_label(clock,"SEC",354,255,56);
    clock_zone=pixel_label(clock,"",76,251,100);clock_status=pixel_label(clock,"CONNECT TO WI-FI",60,95,360);
    box(clock,72,293,336,3,PIXEL_YELLOW);
    clock_alarm_caption=pixel_label(clock,"ALARM OFF",84,304,240);lv_obj_set_style_text_align(clock_alarm_caption,LV_TEXT_ALIGN_LEFT,0);
    clock_alarm_value=pixel_button(clock,"SET ALARM",80,333,232,50,CLOCK_ALARM);lv_obj_set_style_text_align(lv_obj_get_child(clock_alarm_value,0),LV_TEXT_ALIGN_LEFT,0);
    clock_alarm_toggle=pixel_button(clock,"OFF",328,337,72,42,ALARM_TOGGLE);clock_tabs(clock,0);
    for(unsigned index=0;index<2;++index){
        auto *page=pages[index?TIMER_EDIT:ALARM_EDIT];pixel_label(page,index?"TIMER":"ALARM",80,66,320,&orbit_pixel_28);
        pixel_label(page,index?"MINUTE":"HOUR",94,126,128);pixel_label(page,index?"SECOND":"MINUTE",258,126,128);
        auto **fields=index?timer_fields:alarm_fields;
        fields[0]=pixel_button(page,"00",94,163,128,74,EDIT_HOUR,&orbit_pixel_50);fields[1]=pixel_button(page,"00",258,163,128,74,EDIT_MINUTE,&orbit_pixel_50);
        for(auto *b:{fields[0],fields[1]}){lv_obj_set_style_border_color(b,lv_color_hex(PIXEL_YELLOW),0);lv_obj_set_style_border_side(b,LV_BORDER_SIDE_BOTTOM,0);}
        pixel_label(page,":",229,174,22,&orbit_pixel_50);
        edit_minus[index]=pixel_button(page,"-",98,261,64,54,EDIT_MINUS,&orbit_pixel_28);edit_plus[index]=pixel_button(page,"+",318,261,64,54,EDIT_PLUS,&orbit_pixel_28);
        edit_selection[index]=pixel_label(page,index?"MINUTE":"HOUR",164,277,152);
        clock_save_hint[index]=pixel_label(page,"",90,103,300);
        clock_tabs(page,index?2:1);
        if(index){timer_run=pixel_button(page,"START",80,333,152,48,TIMER_RUN);auto *reset=pixel_button(page,"RESET",248,333,152,48,TIMER_RESET);lv_obj_set_style_border_width(reset,2,0);lv_obj_set_style_border_color(reset,lv_color_hex(PIXEL_YELLOW),0);pixel_selected(timer_run,true);}
        else {auto *save=pixel_button(page,"SAVE",80,333,152,48,ALARM_SAVE);pixel_selected(save,true);alarm_draft_toggle=pixel_button(page,"OFF",272,333,128,48,ALARM_DRAFT_TOGGLE);}
    }
    clock_alert_title=pixel_label(pages[CLOCK_ALERT],"ALARM",60,158,360,&orbit_pixel_28);
    pixel_label(pages[CLOCK_ALERT],"TIME IS UP",80,215,320);
    auto *stop=pixel_button(pages[CLOCK_ALERT],"STOP",128,285,224,64,ALERT_STOP,&orbit_pixel_28);pixel_selected(stop,true);
    // Temporary orientation markers show the finger direction that opened each
    // remaining page. Lines/labels are not touch targets or additional buffers.
    static const lv_point_t leftArrow[]={{278,240},{202,240},{224,218},{202,240},{224,262}};
    static const lv_point_t rightArrow[]={{202,240},{278,240},{256,218},{278,240},{256,262}};
    static const lv_point_t upArrow[]={{240,278},{240,202},{218,224},{240,202},{262,224}};
    const lv_point_t *arrows[]={upArrow,rightArrow,leftArrow};
    const View directions[]={BLANK_DOWN,BLANK_LEFT,BLANK_RIGHT};
    const char *captions[]={"Swiped up","Swiped right","Swiped left"};
    const char *returns[]={"Swipe down to return","Swipe left to return","Swipe right to return"};
    for(unsigned i=0;i<3;++i) {
        if(directions[i]!=BLANK_RIGHT)continue;
        auto *page=pages[directions[i]],*line=lv_line_create(page);lv_obj_remove_style_all(line);
        lv_line_set_points(line,arrows[i],5);lv_obj_set_style_line_color(line,themed_color(FG),0);
        lv_obj_set_style_line_width(line,4,0);lv_obj_set_style_line_rounded(line,true,0);
        lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
        label(page,captions[i],80,152,320,&lv_font_montserrat_18,MUTED);
        label(page,returns[i],60,312,360,&lv_font_montserrat_14,MUTED);
    }
    auto *weather=pages[BLANK_DOWN];
    weather_city=label(weather,"Weather",65,68,350,&metadata_title_font,FG);lv_label_set_long_mode(weather_city,LV_LABEL_LONG_SCROLL_CIRCULAR);
    weather_icon=box(weather,192,106,96,96,BG);lv_obj_set_style_bg_opa(weather_icon,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(weather_icon,themed_color(GREEN),0);
    lv_obj_add_event_cb(weather_icon,draw_weather,LV_EVENT_DRAW_MAIN,nullptr);hidden(weather_icon,true);
    weather_temperature=label(weather,"",80,207,320,&lv_font_montserrat_48,FG);
    weather_condition=label(weather,"Finding your area",50,267,380,&lv_font_montserrat_18,FG);
    weather_range=label(weather,"",80,322,320,&lv_font_montserrat_16,FG);
    weather_details=label(weather,"",55,296,370,&lv_font_montserrat_14,MUTED);
    weather_wind=label(weather,"",74,357,100,&lv_font_montserrat_20,FG);
    weather_humidity=label(weather,"",190,357,100,&lv_font_montserrat_20,FG);
    weather_precip=label(weather,"",306,357,100,&lv_font_montserrat_20,FG);
    const char *metricNames[]={"Wind km/h","Humidity","Precip today"};
    for(unsigned i=0;i<3;++i){weather_metric_labels[i]=label(weather,metricNames[i],74+116*i,385,100,&lv_font_montserrat_14,MUTED);hidden(weather_metric_labels[i],true);}
    weather_age=label(weather,"",130,423,220,&lv_font_montserrat_12,MUTED);
    auto *launch=pages[BLANK_LEFT];
    label(launch,"AirPlay",80,100,320,&lv_font_montserrat_24,FG);
    auto *launch_icon=lv_img_create(launch);const auto *airplay=&airplay_launch_image;
    lv_img_set_src(launch_icon,airplay);lv_img_set_pivot(launch_icon,0,0);
    lv_img_set_zoom(launch_icon,256);lv_obj_set_pos(launch_icon,220,160);
    lv_obj_set_style_img_recolor(launch_icon,themed_color(GREEN),0);lv_obj_set_style_img_recolor_opa(launch_icon,LV_OPA_COVER,0);
    lv_obj_clear_flag(launch_icon,LV_OBJ_FLAG_CLICKABLE);
    label(launch,"Apple Music",80,220,320,&lv_font_montserrat_20,FG);
    iphone_button=button(launch,"Play on S3",140,266,200,64,LAUNCH_IPHONE,GREEN);
    auto *launch_text=lv_obj_get_child(iphone_button,0);
    lv_obj_set_style_text_font(launch_text,&lv_font_montserrat_18,0);lv_obj_set_style_text_color(launch_text,themed_color(0x193225),0);
    iphone_status=label(launch,"",80,348,320,&lv_font_montserrat_14,MUTED);
    label(launch,"Swipe left to return",80,400,320,&lv_font_montserrat_14,MUTED);
    auto *p=pages[PLAYER];
    if(!ORBIT_LIVE) make_cover();cover=lv_img_create(p);
    if(cover_pixels) lv_img_set_src(cover,&cover_image);
    else lv_img_set_src(cover,LV_SYMBOL_AUDIO);
    lv_obj_set_pos(cover,155,102);lv_obj_add_flag(cover,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(cover,click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(ARTWORK_TOGGLE)));
    make_vinyl();vinyl=lv_img_create(p);
    if(vinyl_pixels) lv_img_set_src(vinyl,&vinyl_image);
    else lv_img_set_src(vinyl,LV_SYMBOL_AUDIO);
    lv_obj_set_pos(vinyl,155,102);lv_img_set_pivot(vinyl,85,85);
    lv_obj_clear_flag(vinyl,LV_OBJ_FLAG_CLICKABLE);
    vinyl_shine=box(p,155,102,170,170,BG);lv_obj_set_style_bg_opa(vinyl_shine,LV_OPA_TRANSP,0);
    lv_obj_add_event_cb(vinyl_shine,draw_vinyl_shine,LV_EVENT_DRAW_MAIN,nullptr);
    tonearm=box(p,275,102,90,145,BG);lv_obj_set_style_bg_opa(tonearm,LV_OPA_TRANSP,0);
    lv_obj_add_event_cb(tonearm,draw_tonearm,LV_EVENT_DRAW_MAIN,nullptr);
    intro_entry=button(p,"Music information",65,276,350,60,SHOW_INTRO);
    lv_obj_set_ext_click_area(intro_entry,0);lv_obj_add_flag(intro_entry,LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_PRESS_LOCK);lv_obj_add_event_cb(intro_entry,intro_entry_hit,LV_EVENT_HIT_TEST,nullptr);lv_obj_set_style_bg_opa(intro_entry,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(intro_entry,LV_OPA_TRANSP,LV_STATE_PRESSED);hidden(lv_obj_get_child(intro_entry,0),true);
    title=metadata_label(p,65,286-metadata_title_top,350,&metadata_title_font,FG);
    artist=metadata_label(p,65,318-metadata_artist_top,350,&metadata_artist_font);
    transport[0]=button(p,LV_SYMBOL_PREV,119,344,64,60,PREVIOUS);
    transport[1]=button(p,LV_SYMBOL_PAUSE,208,342,64,64,PLAY,GREEN);
    lv_obj_set_style_radius(transport[1],LV_RADIUS_CIRCLE,0);
    play_text=lv_obj_get_child(transport[1],0);
    transport[2]=button(p,LV_SYMBOL_NEXT,297,344,64,60,NEXT);
    for(auto *b:transport)lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_montserrat_24,0);
    auto *io=button(p,"I/O",38,orbit_layout::HomeSideTop,orbit_layout::HomeSideSize,orbit_layout::HomeSideSize,SHOW_ROUTES);home_io=io;
    lv_obj_set_style_text_font(lv_obj_get_child(io,0),&lv_font_montserrat_20,0);
    // Match the actual20px ink bounds of the speaker, not font line boxes.
    lv_obj_align(lv_obj_get_child(io,0),LV_ALIGN_CENTER,0,1);
    mute_buttons[0]=button(p,LV_SYMBOL_VOLUME_MID,378,orbit_layout::HomeSideTop,orbit_layout::HomeSideSize,orbit_layout::HomeSideSize,MUTE);
    lv_obj_set_style_bg_opa(mute_buttons[0],LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_opa(mute_buttons[0],LV_OPA_TRANSP,LV_STATE_PRESSED);
    lv_obj_set_style_text_font(lv_obj_get_child(mute_buttons[0],0),&lv_font_montserrat_26,0);
    // LV_SYMBOL_MUTE is a speaker without waves; add an explicit slash so the
    // physical mute state is distinguishable at the panel's actual size.
    static const lv_point_t slash_points[]={{0,18},{18,0}};
    mute_slash=lv_line_create(mute_buttons[0]);
    lv_line_set_points(mute_slash,slash_points,2);
    lv_obj_set_style_line_width(mute_slash,2,0);
    lv_obj_set_style_line_rounded(mute_slash,true,0);
    lv_obj_align(mute_slash,LV_ALIGN_CENTER,1,1);
    lv_obj_clear_flag(mute_slash,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *side_actions[]={io,mute_buttons[0]};
    for(auto *b:side_actions) {
        lv_obj_set_ext_click_area(b,12);
        lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_add_event_cb(b,side_hit,LV_EVENT_HIT_TEST,nullptr);
    }
    progress=lv_bar_create(p);lv_obj_remove_style_all(progress);lv_bar_set_range(progress,0,1000);
    lv_obj_set_pos(progress,136,414);lv_obj_set_size(progress,208,2);
    lv_obj_set_style_bg_color(progress,themed_color(TRACK),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress,themed_color(0x90a699),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress,LV_OPA_COVER,LV_PART_INDICATOR);
    elapsed=label(p,"",136,425,80,&lv_font_montserrat_16);
    duration=label(p,"",264,425,80,&lv_font_montserrat_16);
    lv_obj_set_style_text_align(elapsed,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_style_text_align(duration,LV_TEXT_ALIGN_RIGHT,0);
    wake_button=button(p,"Wake",200,200,80,80,POWER_WAKE,GREEN);
    lv_obj_set_style_radius(wake_button,LV_RADIUS_CIRCLE,0);
    hidden(lv_obj_get_child(wake_button,0),true); // Identity retained; no visible caption.
    lv_obj_add_event_cb(wake_button,draw_wake_power,LV_EVENT_DRAW_MAIN,nullptr);
    p=pages[VOLUME];
    label(p,"VOLUME",90,110,300,&lv_font_montserrat_14);
    // Direct font drawing needs no transparent transform layer (disabled by
    // the pinned vendor configuration), and no temporary scaling framebuffer.
    number=label(p,"38",90,184,300,&lv_font_montserrat_48,FG);
    volume_unit=label(p,"",80,274,320,&lv_font_montserrat_14);
    volume_buttons[0]=button(p,"-",108,312,72,72,MINUS,0x17221d);
    auto *mute=button(p,"Mute",204,312,72,72,MUTE,0x17221d);
    mute_buttons[1]=mute;mute_text=lv_obj_get_child(mute,0);
    lv_obj_set_style_radius(mute,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_text_font(mute_text,&lv_font_montserrat_12,0);
    volume_buttons[1]=button(p,"+",300,312,72,72,PLUS,0x17221d);
    volume_symbol(volume_buttons[0],false);volume_symbol(volume_buttons[1],true);
    for(auto *b:volume_buttons) {
        lv_obj_set_style_radius(b,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_border_width(b,1,0);
        lv_obj_set_style_border_color(b,themed_color(MUTED),0);
    }
    lv_obj_set_style_border_width(mute,1,0);
    lv_obj_set_style_border_color(mute,themed_color(MUTED),0);
    volume_hint=label(p,"",80,388,320,&lv_font_montserrat_14);

    back_button(p,164,412,52,BACK);
    p=pages[THEME];
    label(p,"Theme",90,82,300,&lv_font_montserrat_24,FG);
    for(unsigned i=0;i<orbit_theme::Count;i++) {
        auto *b=button(p,"",92+(i%4)*76,130+(i/4)*92,68,80,THEME_BASE+i);
        theme_buttons[i]=b;lv_obj_set_ext_click_area(b,4);
        auto *name=lv_obj_get_child(b,0);text(name,orbit_theme::palettes[i].name);
        lv_obj_set_style_text_font(name,&lv_font_montserrat_14,0);
        lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(name,4,orbit_layout::centered(80,28+10+lv_font_montserrat_14.line_height)+28+10);
        lv_obj_set_width(name,60);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
        theme_swatches[i]=box(b,20,orbit_layout::centered(80,28+10+lv_font_montserrat_14.line_height),28,28,orbit_theme::palettes[i].swatch);
        lv_obj_set_style_radius(theme_swatches[i],LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_border_width(theme_swatches[i],1,0);
        lv_obj_set_style_outline_pad(theme_swatches[i],6,0);
    }
    auto *brightness_label=label(p,"Brightness",100,344,128,&lv_font_montserrat_16,FG);
    lv_obj_set_style_text_align(brightness_label,LV_TEXT_ALIGN_LEFT,0);
    brightness_value=label(p,"",288,346,52,&lv_font_montserrat_14,FG);
    for(unsigned i=0;i<2;++i) {
        auto *b=button(p,i?"+":"-",234+i*108,324,56,56,i?BRIGHTNESS_PLUS:BRIGHTNESS_MINUS,0x17221d);
        brightness_buttons[i]=b;lv_obj_set_ext_click_area(b,12);lv_obj_add_flag(b,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_add_event_cb(b,pixel_hit,LV_EVENT_HIT_TEST,nullptr);
        lv_obj_set_style_radius(b,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,themed_color(TRACK),0);
        lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_montserrat_26,0);lv_obj_center(lv_obj_get_child(b,0));
    }
    appearance_hint=label(p,"",120,389,240,&lv_font_montserrat_12);
    back_button(p,164,412,52,THEME_BACK);
    p=pages[ABOUT];
    label(p,"About",90,78,300,&lv_font_montserrat_24,FG);
    auto *details=box(p,78,126,324,218,0x17221d);
    lv_obj_set_style_radius(details,12,0);
    const char *keys[]={"Screen IP","S3 IP","Connection","Power","Wi-Fi","Bluetooth","Input","Firmware"};
    for(unsigned i=0;i<8;++i) {
        auto *key=label(details,keys[i],14,12+i*26,112,&lv_font_montserrat_14);
        lv_obj_set_style_text_align(key,LV_TEXT_ALIGN_LEFT,0);
        if(i==6)about_input_key=key;
        about_values[i]=label(details,"--",128,12+i*26,182,&lv_font_montserrat_14,FG);
        lv_obj_set_style_text_align(about_values[i],LV_TEXT_ALIGN_RIGHT,0);
        lv_label_set_long_mode(about_values[i],LV_LABEL_LONG_DOT);
    }
    about_status=label(p,"",78,352,324,&lv_font_montserrat_12);
    lv_obj_set_height(about_status,34);lv_label_set_long_mode(about_status,LV_LABEL_LONG_DOT);
    back_button(p,164,412,52,ABOUT_BACK);
    p=pages[ROUTING];
    auto *about=button(p,"i",36,208,64,64,SHOW_ABOUT);
    auto *info=lv_obj_get_child(about,0);lv_obj_set_size(info,26,26);
    lv_obj_align(info,LV_ALIGN_TOP_MID,0,orbit_layout::centered(64,26+8+lv_font_montserrat_12.line_height));
    lv_obj_set_style_border_width(info,1,0);lv_obj_set_style_border_color(info,themed_color(FG),0);
    lv_obj_set_style_radius(info,LV_RADIUS_CIRCLE,0);lv_obj_set_style_pad_top(info,2,0);
    label(about,"About",0,orbit_layout::centered(64,26+8+lv_font_montserrat_12.line_height)+26+8,64,&lv_font_montserrat_12);
    // Same side action position as the player's mute control. Local and
    // available offline; it does not issue a device command.
    auto *theme=button(p,LV_SYMBOL_TINT,380,208,64,64,SHOW_THEME);
    auto *theme_icon=lv_obj_get_child(theme,0);lv_obj_set_size(theme_icon,26,26);
    lv_obj_align(theme_icon,LV_ALIGN_TOP_MID,0,orbit_layout::centered(64,26+8+lv_font_montserrat_12.line_height));
    lv_obj_set_style_text_font(theme_icon,&lv_font_montserrat_24,0);
    lv_obj_set_style_pad_top(theme_icon,1,0);
    label(theme,"Theme",0,orbit_layout::centered(64,26+8+lv_font_montserrat_12.line_height)+26+8,64,&lv_font_montserrat_12);
    standby_button=button(p,"Standby",208,14,64,64,POWER_STANDBY);
    lv_obj_set_style_bg_opa(standby_button,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_opa(standby_button,LV_OPA_TRANSP,LV_STATE_PRESSED);
    hidden(lv_obj_get_child(standby_button,0),true);
    auto *standby_icon=label(standby_button,LV_SYMBOL_POWER,0,0,64,&lv_font_montserrat_24,FG);
    lv_obj_center(standby_icon);
    label(p,"Input / Output",70,78,340,&lv_font_montserrat_24,FG);
    auto *current=box(p,112,118,256,34,0x15241d);
    lv_obj_set_style_radius(current,9,0);
    route_source_icon=lv_img_create(current);lv_obj_set_pos(route_source_icon,12,5);
    lv_obj_set_style_img_recolor(route_source_icon,themed_color(GREEN),0);
    lv_obj_set_style_img_recolor_opa(route_source_icon,LV_OPA_COVER,0);
    route_source=label(current,"",38,8,210,&lv_font_montserrat_14,GREEN);
    lv_label_set_long_mode(route_source,LV_LABEL_LONG_DOT);
    label(p,"INPUT",112,162,54,&lv_font_montserrat_12);
    const char *symbols[]={"",LV_SYMBOL_BLUETOOTH,LV_SYMBOL_AUDIO,LV_SYMBOL_VIDEO,LV_SYMBOL_USB};
    // Network is report-only; streams take over from their sending application.
    for(int i=1;i<5;i++) {
        int j=i-1;
        input_btns[i]=route_button(p,symbols[i],inputs[i],nullptr,
            112+(j%2)*132,184+(j/2)*68,INPUT_BASE+i,56,
            i>1?s3::source_icon_image(s3::physical_source_icon(i)):nullptr);
    }
    label(p,"OUTPUT",112,319,65,&lv_font_montserrat_12);
    output_btns[0]=route_button(p,"","Analog","RCA + XLR",112,340,OUTPUT_BASE,56,
        s3::source_icon_image(s3::SourceIcon::Analog));
    output_btns[1]=route_button(p,"","Optical",nullptr,244,340,OUTPUT_BASE+1,56,
        s3::source_icon_image(s3::SourceIcon::Optical));
    back_button(p,164,412,52,BACK);
    auto *status=box(root,225,448,30,24,BG);
    lv_obj_set_style_bg_opa(status,LV_OPA_TRANSP,0);
    status_icon=lv_img_create(status);
    lv_img_set_src(status_icon,orbit_status::icon_image(orbit_status::Icon::Demo));
    lv_obj_set_pos(status_icon,3,0);
    lv_obj_set_style_img_recolor_opa(status_icon,LV_OPA_COVER,0);
    lv_obj_clear_flag(status_icon,LV_OBJ_FLAG_CLICKABLE);
    status_target=box(root,200,440,80,40,BG);
    lv_obj_set_style_bg_opa(status_target,LV_OPA_TRANSP,0);
    lv_obj_add_flag(status_target,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(status_target,footer_hit,LV_EVENT_HIT_TEST,nullptr);
    lv_obj_add_event_cb(status_target,click,LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<intptr_t>(SHOW_ABOUT)));
    source_row=box(root,90,39,300,24,BG);lv_obj_set_style_bg_opa(source_row,LV_OPA_TRANSP,0);
    lv_obj_set_flex_flow(source_row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(source_row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(source_row,8,0);
    source_icon=lv_img_create(source_row);
    lv_obj_set_style_img_recolor(source_icon,themed_color(GREEN),0);
    lv_obj_set_style_img_recolor_opa(source_icon,LV_OPA_COVER,0);
    source=label(source_row,"",0,0,1,&lv_font_montserrat_12,GREEN);
    lv_label_set_long_mode(source,LV_LABEL_LONG_DOT);
    chip=button(root,LV_SYMBOL_VOLUME_MID"  38",184,64,112,36,SHOW_VOLUME);
    lv_obj_set_ext_click_area(chip,12);
    lv_obj_add_flag(chip,LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(chip,chip_cover_hit,LV_EVENT_HIT_TEST,nullptr);
    chip_text=lv_obj_get_child(chip,0);
    lv_obj_set_style_text_font(chip_text,&lv_font_montserrat_18,0);
    if(ORBIT_LIVE) {state.volume=0;state.input=state.output=-1;state.playing=false;}
    artwork_view=box(root,0,0,480,480,BG);
    lv_obj_add_flag(artwork_view,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(artwork_view,artwork_contact,LV_EVENT_ALL,nullptr);
    lv_obj_add_event_cb(artwork_view,click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(ARTWORK_TOGGLE)));
    artwork_large=lv_img_create(artwork_view);lv_obj_set_pos(artwork_large,80,80);
    lv_img_set_pivot(artwork_large,0,0);lv_img_set_antialias(artwork_large,true);
    lv_obj_clear_flag(artwork_large,LV_OBJ_FLAG_CLICKABLE);
    hidden(artwork_view,true);hidden(artwork_large,true);
    lyrics_button=button(pages[PLAYER],"Lyrics",324,95,44,44,SHOW_LYRICS,BG);
    hidden(lv_obj_get_child(lyrics_button,0),true);lv_obj_set_ext_click_area(lyrics_button,8);
    lv_obj_set_style_bg_opa(lyrics_button,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(lyrics_button,LV_OPA_TRANSP,LV_STATE_PRESSED);
    lv_obj_set_style_border_width(lyrics_button,0,0);
    auto *lyrics_mark=lv_img_create(lyrics_button);lv_img_set_src(lyrics_mark,&lyrics_icon_image);lv_obj_set_pos(lyrics_mark,8,8);
    lv_obj_set_style_img_recolor(lyrics_mark,themed_color(FG),0);lv_obj_set_style_img_recolor_opa(lyrics_mark,LV_OPA_COVER,0);lv_obj_clear_flag(lyrics_mark,LV_OBJ_FLAG_CLICKABLE);
    hidden(lyrics_button,true);
    orbit_appearance::begin(ORBIT_THEME);
    apply_theme(orbit_appearance::theme());render_appearance();
    orbit_display_set_brightness(orbit_appearance::brightness());
    show(PLAYER);lv_timer_create(tick,100,nullptr);
    if(ORBIT_ANIMATION_INTERVAL_MS>0) lv_timer_create(animate_vinyl,ORBIT_ANIMATION_INTERVAL_MS,nullptr);
}

#if ORBIT_INTRO_MOCK
// Host/local preview controls only. No on-device settings or network side effect.
void orbit_ui_intro_mock_scenario(unsigned scenario){
    if(scenario>4)return;
    if(!intro_client)intro_client=orbit_intro_runtime::client();
    if(intro_client){intro_client->scenario(scenario);if(state.view==INTRO_SOURCES)show(INTRO_READ);if(state.view==INTRO_READ)begin_intro_request();render_intro();}
}
unsigned orbit_ui_intro_mock_starts(){return intro_client?intro_client->starts():0;}
#endif
