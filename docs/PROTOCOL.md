# Protocol and compatibility

Fosi S3 Controller uses the S3's local StreamSDK/nSDK API. Its independent C++ implementation
was informed by these community integrations:

- [faiz-m/ha_fosi_s3](https://github.com/faiz-m/ha_fosi_s3), reviewed at
  `aba43168c633e488cce7c12c86afc663b061ec2a`.
- [zzznz27/Fosi-S3](https://github.com/zzznz27/Fosi-S3), reviewed at
  `3124807ce1230225e65ac7571ab9b4f54b041cae`.

These are protocol references; their implementation code is not bundled here.
The official [LilyGO T-RGB repository](https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB)
provides the display board/driver. Pinning and the local driver patch are part of
the build, rather than an automatic upgrade to its latest branch.

Discovery uses Cast mDNS plus product/API identity validation, not a subnet scan.
A manual origin is available when multicast discovery is unavailable. HTTP and
HTTPS availability can differ between S3 firmware versions. HTTPS needs a trusted
CA; certificate verification is never disabled.

Input/output selection, volume, power and playback are read back after commands.
Toggle commands are not automatically retried. Controls depend on reported
capabilities; selecting an input does not prove a signal is present. Bluetooth
link status is read separately. HDMI/optical signal lock and Bluetooth transport
support are not inferred from source selection.

Artwork and timing come from metadata exposed by the S3. AirPlay itself is
initiated by a sender. No local S3 lyrics endpoint has been established; timed
lyrics come from external matching. Failed or ambiguous matches hide the lyrics
entry. The optional phone-launch feature triggers a configured iPhone Shortcut;
it does not make this display an AirPlay sender.

External services: [ipwhois](https://ipwhois.io/),
[Open-Meteo](https://open-meteo.com/), [LRCLIB](https://lrclib.net/docs),
[LrcAPI](https://github.com/HisAtri/LrcApi), and
[Bark](https://github.com/Finb/bark-server). Weather data use
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), formatted by this client;
IP location and forecasts are estimates. Firmware licensing does not grant rights
to third-party music, lyrics, artwork, trademarks or hosted services.
