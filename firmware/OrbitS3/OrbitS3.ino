/**
 * Orbit / S3 — LVGL round-screen controller; demo and live build modes.
 * Board startup follows LilyGO's MIT-licensed lv_helloworld example:
 * https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB/blob/main/examples/lv_helloworld/lv_helloworld.ino
 * Original startup example: Lewis He, Copyright (c) 2024 Shenzhen Xin Yuan
 * Electronic Technology Co., Ltd. See upstream LICENSE.
 */
#include <LilyGo_RGBPanel.h>
#include "orbit_display.h"
#include <esp_heap_caps.h>
#include "orbit_ui.h"
#include "config.h"
#include "s3_client.h"
#include "iphone_trigger.h"
#include "weather.h"
#include "lyrics.h"
#include <freertos/task.h>

LilyGo_RGBPanel panel;

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
    // A slow/disconnected debug host must not stall the UI on diagnostic TX.
    Serial.setTxTimeoutMs(1);
#endif
    if(!psramFound() || ESP.getPsramSize()<4*1024*1024 || ESP.getFlashChipSize()<16*1024*1024) {
        while(true) {Serial.println("Expected T-RGB: 16 MB Flash and at least 4 MB working PSRAM.");delay(1000);}
    }
    // Official touch-model detection. Do not assume Full/Half Circle or V1/V2.
#ifdef ORBIT_PANEL_TYPE
    const bool panelReady=panel.begin(ORBIT_PANEL_TYPE);
#else
    const bool panelReady=panel.begin();
#endif
    if (!panelReady) {
        while (true) {
            Serial.println("T-RGB initialization failed; verify board variant.");
            delay(1000);
        }
    }
    if(!orbit_display_begin(panel)) {
        while(true) {Serial.println("480x480 display/buffer initialization failed.");delay(1000);}
    }
    Serial.printf("T-RGB: touch=%s, PSRAM=%u bytes, Flash=%u bytes\n",
        panel.getTouchModelName(),unsigned(ESP.getPsramSize()),unsigned(ESP.getFlashChipSize()));
    orbit_ui_create();
    if(ORBIT_LIVE) {orbit_weather::begin();orbit_lyrics::begin();s3::begin();}
}

void loop() {
    static uint32_t worstUiMs=0,lastReport=0;
    uint32_t start=millis();lv_timer_handler();
    orbit_display_tick();
    orbit_iphone::serial_tick();
    uint32_t elapsed=millis()-start;if(elapsed>worstUiMs) worstUiMs=elapsed;
    if(ORBIT_DIAGNOSTICS && Serial && uint32_t(millis()-lastReport)>=30000) {
        Serial.printf("Runtime: internal_free=%u min=%u largest=%u psram_free=%u ui_worst_ms=%u vinyl_motion=%u ui_stack_hwm=%u\n",
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),unsigned(worstUiMs),unsigned(orbit_ui_record_motion_active()),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
        worstUiMs=0;lastReport=millis();
    }
    delay(orbit_display_awake()?2:10);
}
