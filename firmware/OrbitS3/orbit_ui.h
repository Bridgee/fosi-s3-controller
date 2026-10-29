#pragma once
#include <lvgl.h>

#if LVGL_VERSION_MAJOR != 8
#error "Orbit UI targets LVGL 8.4.0, as pinned by LilyGO T-RGB's PlatformIO example."
#endif

// Call once after orbit_display_begin(panel). Demo mode is local; live mode receives snapshots from the network worker.
void orbit_ui_create();

// Read on the UI thread only; diagnostic flag, not playback/control state.
bool orbit_ui_record_motion_active();

// Local lyrics reading keeps the display lit, including while playback is paused.
bool orbit_ui_keep_screen_on();

// Display-only state: cached snapshots/commands keep running while painting pauses.
void orbit_ui_set_screen_awake(bool awake);

// UI-thread pointer sample after the wake-only filter, before LVGL dispatch.
// Observes gestures and consumes drag releases via LVGL's existing mechanism.
void orbit_ui_touch_sample(int16_t x,int16_t y,bool pressed);

// Only a configured local alarm/timer can request an automatic display wake.
bool orbit_ui_alert_active();

#if defined(ORBIT_INTRO_MOCK) && ORBIT_INTRO_MOCK
// Local preview/test harness only, never playback or network control.
void orbit_ui_intro_mock_scenario(unsigned scenario);
unsigned orbit_ui_intro_mock_starts();
#endif
