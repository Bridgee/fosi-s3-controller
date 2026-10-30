"""Exercise generated fonts and real LVGL fallback with ASan/UBSan."""
from pathlib import Path
import subprocess
import json
import re
import tempfile

root = Path(__file__).resolve().parents[1]
lvgl = root / ".pio/libdeps/orbit-live-check/lvgl"
golden=root / "assets/fonts/metadata-fingerprints.json"
if not golden.is_file():
    raise AssertionError("Required original font baseline is missing")
expected=json.loads(golden.read_text())["fonts"]
if set(expected)!={"orbit_metadata_24","orbit_metadata_12","orbit_metadata_latin_24","orbit_metadata_latin_14"}:
    raise AssertionError("Incomplete original font baseline")
with tempfile.TemporaryDirectory(prefix="orbit-fonts-") as temp:
    temp = Path(temp)
    (temp / "lv_conf.h").write_text("""
#define LV_COLOR_DEPTH 16
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_MEMCPY_MEMSET_STD 1
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_FONT_COMPRESSED 0
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_14 1
""")
    sources = [root / "tests/metadata_fonts_test.c", root / "firmware/OrbitS3/metadata_font.c",
               *root.glob("firmware/OrbitS3/orbit_metadata_*.c"),root / "firmware/OrbitS3/orbit_numeric_48.c"]
    sources += [lvgl / ("src/" + name) for name in [
        "font/lv_font.c", "font/lv_font_fmt_txt.c", "font/lv_font_montserrat_24.c",
        "font/lv_font_montserrat_14.c", "font/lv_font_montserrat_48.c", "misc/lv_mem.c", "misc/lv_utils.c", "misc/lv_gc.c"]]
    binary = temp / "font-test"
    subprocess.run(["cc", "-std=c99", "-O1", "-g", "-fsanitize=address,undefined",
                    "-DLV_CONF_INCLUDE_SIMPLE", "-DLV_CONF_SUPPRESS_DEFINE_CHECK",
                    "-I" + str(temp), "-I" + str(lvgl), "-I" + str(lvgl.parent),
                    "-I" + str(root / "firmware/OrbitS3"),
                    *map(str, sources), "-o", str(binary)], check=True)
    result=subprocess.run([str(binary)],capture_output=True,text=True)
    if result.returncode:
        print(result.stdout+result.stderr)
        result.check_returncode()
    print(result.stdout,end="")
    actual={name:{"fnv1a64":digest,"glyphs":int(count)} for name,digest,count in
            re.findall(r"FONT_FINGERPRINT (\w+) ([0-9a-f]+) (\d+)",result.stdout)}
    if actual!=expected:
        raise AssertionError("Metadata glyph metrics/pixels differ from original fonts")
    print("PASS: all BMP metrics/coverage and every original bitmap byte unchanged for all 4 metadata fonts")
