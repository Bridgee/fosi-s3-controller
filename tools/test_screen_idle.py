"""Exercise the real idle policy and actual display adapter with mocked hardware."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="screen-idle-") as d:
 w=Path(d)
 (w/"lvgl.h").write_text(r'''#pragma once
#include <stdint.h>
#define LVGL_VERSION_MAJOR 8
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_INDEV_TYPE_POINTER 1
#define LV_INDEV_STATE_PR 1
#define LV_INDEV_STATE_REL 0
struct lv_color_t {uint16_t full;};
struct lv_area_t {int16_t x1,y1,x2,y2;};
struct lv_point_t {int16_t x,y;};
struct lv_timer_t {bool paused=false;};
struct lv_disp_draw_buf_t {};
struct lv_disp_drv_t {int hor_res,ver_res;lv_disp_draw_buf_t *draw_buf;void (*flush_cb)(lv_disp_drv_t*,const lv_area_t*,lv_color_t*);void *user_data;};
struct lv_indev_data_t {lv_point_t point;int state;};
struct lv_indev_drv_t {int type;void (*read_cb)(lv_indev_drv_t*,lv_indev_data_t*);void *user_data;};
struct lv_disp_t {lv_timer_t *refr_timer;bool invalidation=true;};
#include <vector>
extern uint32_t fakeNow;extern std::vector<int> calls;
extern lv_disp_drv_t *registeredDriver;extern lv_indev_drv_t *registeredTouch;
extern lv_disp_t fakeDisplay;extern lv_timer_t fakeRefresh;extern int fakeRoot;
inline uint32_t lv_tick_get(){return fakeNow;}
inline void lv_init(){}
inline void lv_disp_draw_buf_init(lv_disp_draw_buf_t*,lv_color_t*,lv_color_t*,unsigned){}
inline void lv_disp_drv_init(lv_disp_drv_t*){}
inline lv_disp_t *lv_disp_drv_register(lv_disp_drv_t *d){registeredDriver=d;fakeDisplay.refr_timer=&fakeRefresh;return &fakeDisplay;}
inline void lv_disp_flush_ready(lv_disp_drv_t*){}
inline void lv_timer_set_period(lv_timer_t*,int){}
inline void lv_indev_drv_init(lv_indev_drv_t*){}
inline lv_indev_drv_t *lv_indev_drv_register(lv_indev_drv_t *d){registeredTouch=d;return d;}
inline void lv_timer_pause(lv_timer_t *t){t->paused=true;calls.push_back(20);}
inline void lv_timer_resume(lv_timer_t *t){t->paused=false;calls.push_back(21);}
inline int *lv_scr_act(){return &fakeRoot;}
inline void lv_disp_enable_invalidation(lv_disp_t *d,bool en){d->invalidation=en;calls.push_back(en?25:24);}
inline void lv_obj_invalidate(int*){if(fakeDisplay.invalidation){fakeRefresh.paused=false;calls.push_back(22);}}
inline void lv_refr_now(lv_disp_t *d){if(d->refr_timer->paused)__builtin_trap();calls.push_back(23);}
''')
 (w/"LilyGo_RGBPanel.h").write_text(r'''#pragma once
#include <stdint.h>
#include <vector>
extern std::vector<int> calls;extern bool fakePressed;extern int16_t fakeX,fakeY;
class LilyGo_RGBPanel {
public:
 uint8_t brightness=12;
 int width(){return 480;}int height(){return 480;}
 void pushColors(int,int,int,int,uint16_t*){}
 unsigned getPoint(int16_t *x,int16_t *y,unsigned){*x=fakeX;*y=fakeY;return fakePressed?1:0;}
 void setBrightness(uint8_t n){brightness=n;calls.push_back(100+n);}
 uint8_t getBrightness(){return brightness;}
};
''')
 (w/"esp_heap_caps.h").write_text(r'''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern void *heapBuffer;
inline void *heap_caps_malloc(size_t n,int){heapBuffer=malloc(n);return heapBuffer;}
''')
 flags=["c++","-std=c++11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-DORBIT_BUILD_CHECK=1","-I"+str(w),"-I"+str(r/"firmware/OrbitS3")]
 for name,extra in [("screen_idle_test",[]),("screen_idle_display_test",[r/"firmware/OrbitS3/orbit_display.cpp"])]:
  binary=w/name
  subprocess.run([*flags,str(r/"tests"/(name+".cpp")),*[str(p) for p in extra],"-o",str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
