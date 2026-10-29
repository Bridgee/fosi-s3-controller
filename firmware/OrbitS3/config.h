#pragma once
#ifndef ORBIT_LIVE
#define ORBIT_LIVE 0
#endif
#if !defined(ORBIT_BUILD_CHECK) && __has_include("config_local.h")
#include "config_local.h"
#endif
#ifndef ORBIT_WIFI_SSID
#define ORBIT_WIFI_SSID ""
#endif
#ifndef ORBIT_WIFI_PASSWORD
#define ORBIT_WIFI_PASSWORD ""
#endif
#ifndef ORBIT_S3_URL
#define ORBIT_S3_URL ""
#endif
#ifndef ORBIT_S3_CA_CERT
#define ORBIT_S3_CA_CERT ""
#endif
#ifndef ORBIT_ARTWORK_CA_CERT
#define ORBIT_ARTWORK_CA_CERT ORBIT_S3_CA_CERT
#endif
#ifndef ORBIT_DIAGNOSTICS
#define ORBIT_DIAGNOSTICS 1
#endif
#ifndef ORBIT_VOLUME_CEILING
// volumeMap is a scale, not the user's Maximum Volume setting.
// Until that setting is verified/configured, live volume remains read-only.
#define ORBIT_VOLUME_CEILING -1
#endif
#if ORBIT_VOLUME_CEILING < -1 || ORBIT_VOLUME_CEILING > 1000
#error "ORBIT_VOLUME_CEILING must be -1 (read-only) or a verified volume index"
#endif
#ifndef ORBIT_ARTWORK_ENABLED
#define ORBIT_ARTWORK_ENABLED 1
#endif
#ifndef ORBIT_ANIMATION_INTERVAL_MS
#define ORBIT_ANIMATION_INTERVAL_MS 200
#endif
#ifndef ORBIT_THEME
// Startup palette: 0 Green, 1 Orange, 2 Blue, 3 Gray, 4 Red, 5 Cream, 6 Ice, 7 White. Used only when no saved theme exists.
#define ORBIT_THEME 0
#endif
#if ORBIT_THEME < 0 || ORBIT_THEME > 7
#error "ORBIT_THEME must be 0..7"
#endif
#if ORBIT_ANIMATION_INTERVAL_MS != 0 && ORBIT_ANIMATION_INTERVAL_MS < 100
#error "Use an animation interval >=100 ms, or 0 to disable it"
#endif


#ifndef ORBIT_SCREEN_IDLE_MS
// Touch-only timeout; 0 disables automatic display off. Network/S3 remain active.
#define ORBIT_SCREEN_IDLE_MS 30000
#endif
#if ORBIT_SCREEN_IDLE_MS != 0 && (ORBIT_SCREEN_IDLE_MS < 1000 || ORBIT_SCREEN_IDLE_MS > 3600000)
#error "ORBIT_SCREEN_IDLE_MS must be 0 or 1000..3600000"
#endif

// UTC is synchronized independently; IP geolocation selects local IANA rules.
#ifndef ORBIT_NTP_SERVER1
#define ORBIT_NTP_SERVER1 "pool.ntp.org"
#endif
#ifndef ORBIT_NTP_SERVER2
#define ORBIT_NTP_SERVER2 "time.cloudflare.com"
#endif

// Optional phone launch sender. No startup/reconnect playback trigger.
#ifndef ORBIT_IPHONE_TRIGGER_ENABLED
#define ORBIT_IPHONE_TRIGGER_ENABLED 0
#endif

#ifndef ORBIT_IPHONE_SERIAL_TEST
#define ORBIT_IPHONE_SERIAL_TEST 0
#endif

// Approximate city from the display's public IP; background weather, no API key.
#ifndef ORBIT_WEATHER_ENABLED
#define ORBIT_WEATHER_ENABLED 1
#endif

#ifndef ORBIT_LYRICS_ENABLED
#define ORBIT_LYRICS_ENABLED 1
#endif

// Test-only local introduction UI. Standard firmware keeps this disabled.
#ifndef ORBIT_INTRO_MOCK
#define ORBIT_INTRO_MOCK 0
#endif

// Real OpenAI transport is opt-in and independent of local UI samples.
#ifndef ORBIT_AI_ENABLED
#define ORBIT_AI_ENABLED 0
#endif
#ifndef ORBIT_AI_TRANSPORT_CHECK
#define ORBIT_AI_TRANSPORT_CHECK 0
#endif
#if ORBIT_AI_ENABLED != 0 && ORBIT_AI_ENABLED != 1
#error "ORBIT_AI_ENABLED must be0 or1"
#endif

#if ORBIT_INTRO_MOCK && ORBIT_AI_ENABLED
#error "Local AI samples and live provider mode are mutually exclusive"
#endif
