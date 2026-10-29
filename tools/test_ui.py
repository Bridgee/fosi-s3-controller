"""Run actual LVGL UI code headlessly with a simulated S3 snapshot (not hardware)."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

root=Path(__file__).resolve().parents[1]
(root/"artifacts").mkdir(exist_ok=True)
lvgl=root/".pio/libdeps/orbit-live-check/lvgl"
with tempfile.TemporaryDirectory(prefix="orbit-ui-") as temp:
    work=Path(temp)
    (work/"lv_fault_alloc.h").write_text("""
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void *orbit_test_lv_alloc(size_t);
void orbit_test_lv_free(void *);
#ifdef __cplusplus
}
#endif
""")
    (work/"lv_conf.h").write_text("""
#define LV_COLOR_DEPTH 16
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "lv_fault_alloc.h"
#define LV_MEM_CUSTOM_ALLOC orbit_test_lv_alloc
#define LV_MEM_CUSTOM_FREE orbit_test_lv_free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_MEMCPY_MEMSET_STD 1
#define LV_USE_LOG 0
#define LV_USE_QRCODE 1
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_FONT_COMPRESSED 0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_48 1
""")
    common=["-O1","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer",
            "-DLV_CONF_INCLUDE_SIMPLE","-DLV_CONF_SUPPRESS_DEFINE_CHECK","-DORBIT_LIVE=1","-DORBIT_BUILD_CHECK=1","-DORBIT_INTRO_MOCK=1","-DORBIT_AI_RUNTIME_HOST_TEST=1",
            "-I"+str(root/".pio/libdeps/orbit-live-check/ArduinoJson/src"),
            "-I"+str(work),"-I"+str(root/"tests/fakes"),"-I"+str(lvgl),"-I"+str(root/"firmware/OrbitS3")]
    sources=sorted(lvgl.glob("src/**/*.c"))
    sources+=sorted(root.glob("firmware/OrbitS3/orbit_metadata_*.c"))
    sources+=[root/"firmware/OrbitS3/metadata_font.c",root/"firmware/OrbitS3/metadata_text.cpp",root/"firmware/OrbitS3/orbit_ui.cpp",root/"firmware/OrbitS3/appearance_settings.cpp",
              root/"firmware/OrbitS3/lyrics_parser.cpp",root/"firmware/OrbitS3/weather_labels.cpp",root/"firmware/OrbitS3/weather_icon.cpp",root/"firmware/OrbitS3/weather_assets.cpp",root/"firmware/OrbitS3/time_tools.cpp",root/"firmware/OrbitS3/timezone_data.cpp",root/"firmware/OrbitS3/pixel_clock_font.cpp",root/"firmware/OrbitS3/intro_icons.cpp",root/"firmware/OrbitS3/intro_ai.cpp",root/"firmware/OrbitS3/intro_ai_mock.cpp",root/"firmware/OrbitS3/intro_ai_async.cpp",root/"firmware/OrbitS3/intro_ai_runtime.cpp",root/"tests/fakes/intro_async_ui.cpp",root/"firmware/OrbitS3/source_icons.cpp",root/"firmware/OrbitS3/status_icons.cpp",root/"tests/ui_smoke_test.cpp"]
    def compile_one(item):
        index,path=item;obj=work/f"{index}.o"
        command=["c++" if path.suffix==".cpp" else "cc",
                        "-std=c++11" if path.suffix==".cpp" else "-std=c99",
                        *common,"-c",str(path),"-o",str(obj)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:
            print(result.stderr,flush=True)
            result.check_returncode()
        return str(obj)
    with ThreadPoolExecutor(max_workers=4) as pool:
        objects=list(pool.map(compile_one,enumerate(sources)))
    binary=work/"ui-test"
    subprocess.run(["c++","-fsanitize=address,undefined",*objects,"-o",str(binary)],check=True)
    player=work/"player.ppm";theme=work/"theme.ppm";routing=work/"routing.ppm";about=work/"about.ppm"
    volume=work/"volume.ppm";muted=work/"muted.ppm";volume_muted=work/"volume-muted.ppm";waiting=work/"waiting.ppm"
    bluetooth_about=work/"bluetooth-about.ppm"
    hdmi_about=work/"hdmi-about.ppm";optical_about=work/"optical-about.ppm"
    vinyl_playing=work/"vinyl-playing.ppm";vinyl_paused=work/"vinyl-paused.ppm"
    themes=work/"themes";themes.mkdir()
    power=work/"power";power.mkdir()
    metadata=work/"metadata.ppm"
    subprocess.run([str(binary),str(player),str(theme),str(routing),str(about),str(volume),str(muted),str(volume_muted),str(waiting),str(vinyl_playing),str(vinyl_paused),str(bluetooth_about),str(hdmi_about),str(optical_about),str(themes),str(power),str(metadata)],check=True)
    Image.open(metadata).save(root/"artifacts/native-latin-metadata-preview.png")
    for source,name in [(player,"native-player-preview.png"),(theme,"native-theme-preview.png"),(routing,"native-routing-preview.png"),(about,"native-about-preview.png"),(volume,"native-volume-readonly-preview.png"),(muted,"native-muted-preview.png"),(volume_muted,"native-volume-muted-preview.png"),(waiting,"native-footer-waiting-preview.png"),(vinyl_playing,"native-vinyl-playing-preview.png"),(vinyl_paused,"native-vinyl-paused-preview.png"),(bluetooth_about,"native-bluetooth-about-preview.png"),(hdmi_about,"native-hdmi-about-preview.png"),(optical_about,"native-optical-about-preview.png")]:
        Image.open(source).save(root/"artifacts"/name)

    for source in themes.glob("*.ppm"):
        Image.open(source).save(root/"artifacts"/("theme-"+source.stem+".png"))
    # Contact sheet for visual review of all quantized palettes, not panel calibration.
    import json
    palettes=json.loads((root/"assets/themes.json").read_text())
    sheet=Image.new("RGB",(480*4,480*2))
    for i,palette in enumerate(palettes):
        sheet.paste(Image.open(themes/(palette["id"]+"-routing.ppm")),((i%4)*480,(i//4)*480))
    sheet.save(root/"artifacts/eight-themes-native-preview.png")

    for source in power.glob("*.ppm"):
        Image.open(source).save(root/"artifacts"/("native-power-"+source.stem+".png"))

    # Frames captured without advancing the test clock during file output. This
    # is actual native LVGL raster output with mocked time, not a board FPS claim.
    motion=[Image.open(p).copy() for p in sorted(power.glob("clock-motion-*.ppm"))]
    if motion:
        motion[0].save(root/"artifacts/native-clock-slide.gif",save_all=True,append_images=motion[1:],duration=[20]*(len(motion)-1)+[1200],loop=0)
