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
alarm times, enable states and timer presets are saved; running countdowns restart
as idle after power loss.

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

AI is **off by default**. The online and local browser simulators always use
synthetic responses and never need a key. The firmware uses OpenAI Responses API,
`gpt-6.1-terra`, English output, no additional reasoning, one web search at most,
and short structured text. Check model access and available API credits for your
key's project before enabling it.

1. Create the ignored private configuration beside the example:

   ```sh
   cp firmware/OrbitS3/ai_local.example.h firmware/OrbitS3/ai_local.h
   ```

2. Edit `ai_local.h` locally. Put your project-scoped key inside the quotes of
   `ORBIT_AI_API_KEY`. Set `ORBIT_AI_CA_CERT` to verified PEM root CA certificates
   trusted for `api.openai.com`. The template shows portable adjacent C strings
   with explicit `\n` escapes and continued macro lines. Keep TLS hostname and
   certificate verification enabled; there is no insecure fallback.

3. In your ignored `config_local.h`, explicitly enable AI, then build:

   ```c
   #define ORBIT_AI_ENABLED 1
   ```

   ```sh
   pio run -e orbit-live
   ```

Supplying `ai_local.h` alone does not enable AI. The normal disabled firmware
shows "Introductions are off". Each deliberate Artist, Release or Song selection
can send the captured track metadata to OpenAI; there is no automatic boot or
reconnect request. Three completed topic results and their references are cached
in RAM for the current track. Back reuses them; a confirmed track change clears
them. Canceling locally cannot guarantee that remote work or billing stops.

Version 0.1.58 has been exercised on the target display with real Artist, Release
and Song responses, source references, cancellation, track changes and cache reuse.
Content accuracy, coverage and response time still depend on the provider and music metadata.

### Keep credentials private

Commit only the empty `*.example.h` templates. Git ignores private `*_local.h`
files, environment files, build directories, binaries and local logs. Run
`python tools/check_repository.py` in the public checkout before publishing.
Ignoring a file does not remove an already-tracked file or its Git history.

A configured firmware binary or ELF contains your key: keep it, flash backups,
build logs and diagnostic responses local. Do not attach them to GitHub releases,
issues or screenshots. Public CI uses only credential-free verification targets;
GitHub Pages publishes a separate static simulator inventory. Neither needs an
API key in repository files, Actions secrets or browser code. If a key is ever
published, revoke it and create a replacement; deleting a file is insufficient.

Weather/location contact ipwho.is and Open-Meteo. Lyrics send track metadata to
LRCLIB and, when needed, LrcAPI. Phone launch sends a token-bearing request to
Bark. These features depend on external services; consult their current terms.
