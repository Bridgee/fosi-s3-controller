# Configuration

Copy `firmware/OrbitS3/config_local.example.h` to `config_local.h` in the same
folder. Private `*_local.h` files are ignored by Git. Never share a configured
firmware binary: credentials are compiled into it.

| Setting | Default | Purpose |
| --- | --- | --- |
| `ORBIT_WIFI_SSID`, `ORBIT_WIFI_PASSWORD` | Empty | Display's 2.4 GHz Wi-Fi network |
| `ORBIT_S3_URL` | Empty | Auto discovery; otherwise a verified HTTP/HTTPS origin |
| `ORBIT_S3_CA_CERT` | Empty | Trusted PEM CA for a manual HTTPS S3 endpoint |
| `ORBIT_VOLUME_CEILING` | `-1` | Read-only; set a verified local ceiling to allow writes |
| `ORBIT_ARTWORK_ENABLED` | `1` | Download/decode supported artwork |
| `ORBIT_ANIMATION_INTERVAL_MS` | `200` | Vinyl animation period; `0` disables it |
| `ORBIT_SCREEN_IDLE_MS` | `30000` | Inactivity before display-off; `0` disables it |
| `ORBIT_WEATHER_ENABLED` | `1` | Weather page; location is also used by the clock |
| `ORBIT_LYRICS_ENABLED` | `1` | Look up synchronized lyrics from track metadata |
| `ORBIT_DIAGNOSTICS` | `1` | Serial resource reports |

Theme and brightness are saved when changed. The screen gets its address by
DHCP. About shows the screen and S3 addresses separately. Volume writes also
respect the S3's reported maximum; the controller never changes that setting.

Clock timezone and weather location share approximate public-IP geolocation.
SNTP supplies UTC; bundled IANA rules apply local daylight saving time. An
unresolved location is not replaced with a guessed timezone. VPN/ISP routing can
place the display in another city. Alarm and timer reminders are visual only;
a running timer does not survive power loss.

## Optional iPhone launch

`ORBIT_IPHONE_TRIGGER_ENABLED` defaults to `0`. To enable, copy
`iphone_trigger_local.example.h` to `iphone_trigger_local.h`, fill in a Bark
device key, and set `ORBIT_IPHONE_TRIGGER_ENABLED 1` in `config_local.h`.

On a compatible iPhone, create an automatically running Shortcut triggered by a
Bark notification whose **message contains `FOSI_S3_START`**. Its actions should
set the playback destination to FOSI-S3, wait briefly, and play a chosen playlist.
Verify this while the phone is locked before relying on the display button.
Receiving a notification alone is not proof that playback started. Behavior
depends on iOS, permissions and Shortcut configuration; HomeKit is not required.

The display sends a single authenticated HTTPS push per accepted tap and does
not retry an uncertain delivery. It wakes a sleeping S3 before sending the push.

## Optional music introductions

AI is **off by default**. `ai_local.h` alone does not enable it. Copy
`ai_local.example.h` to `ai_local.h`, supply your API key and a trusted PEM CA for
`api.openai.com`, then explicitly set `ORBIT_AI_ENABLED 1` in `config_local.h`.
No insecure TLS fallback is available.

The current adapter uses Responses API, model `gpt-6.1-terra`, English output,
no additional reasoning, at most one web-search call, and short structured text.
This provider/model combination has **not been validated with a real request**.
Check account access, current API compatibility and billing before enabling it.
Selecting Artist, Release or Song sends track metadata to the provider. Up to
three completed introductions and their sources are cached in RAM for the
current track; Back reuses them, and a confirmed track change clears the cache.
Canceling the local view cannot guarantee that remote processing/billing stops.

Weather/location contact ipwho.is and Open-Meteo. Lyrics send track metadata to
LRCLIB and, when needed, LrcAPI. Phone launch sends a token-bearing request to
Bark. These features depend on external services; consult their current terms.
