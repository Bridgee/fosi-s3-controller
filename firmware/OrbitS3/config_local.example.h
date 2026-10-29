#pragma once
// Copy to config_local.h (ignored by Git). No trailing slash in the URL.
#define ORBIT_WIFI_SSID "your-2.4GHz-wifi"
#define ORBIT_WIFI_PASSWORD "your-wifi-password"
// Empty = discover S3 over Cast mDNS and verify its read-only API.
// Set a full http:// or verified https:// URL for manual addressing.
#define ORBIT_S3_URL ""
// For https://, supply the device's trusted PEM certificate / CA here.
// Certificate verification is never disabled. HTTP/80 and HTTPS/443 differ
// between the community integrations; choose the endpoint your S3 exposes.
#define ORBIT_S3_CA_CERT ""
// Set only after checking the S3's current Maximum Volume setting. -1 keeps
// live volume read-only. Do not copy a number from an old screenshot blindly.
#define ORBIT_VOLUME_CEILING -1
// Optional: specify the physical board revision if auto-detection is wrong.
// #define ORBIT_PANEL_TYPE LILYGO_T_RGB_2_1_INCHES_FULL_CIRCLE_V2
// #define ORBIT_DIAGNOSTICS 0
// Normal playback: cover downloads enabled; record animation updates every 200ms.
// Conservative troubleshooting: static record and no cover downloads.
// #define ORBIT_ARTWORK_ENABLED 0
// #define ORBIT_ANIMATION_INTERVAL_MS 0
// Default theme before first saved selection: 0 Green, 1 Orange, 2 Blue, 3 Gray, 4 Red, 5 Cream, 6 Ice, 7 White. Switch at I/O > right-side Theme.
// #define ORBIT_THEME 0

// Optional: trusted CA PEM for a different HTTPS artwork host.
// #define ORBIT_ARTWORK_CA_CERT "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"

// Auto screen off after30s without touch;20s=20000,0=disabled.
// First touch wakes only; S3/network operation is independent.
// #define ORBIT_SCREEN_IDLE_MS 30000

// Local time zone follows the same public-IP location as weather, including DST.
// No time-zone setting is required.
// Optional local/reachable NTP servers (hostnames, not URLs).
// #define ORBIT_NTP_SERVER1 "pool.ntp.org"
// #define ORBIT_NTP_SERVER2 "time.cloudflare.com"

// Real OpenAI API remains OFF. Supplying ai_local.h does not turn it on.
// #define ORBIT_AI_ENABLED 0
// Local simulations do not need any key or CA. See docs/CONFIGURATION.md before live use.
