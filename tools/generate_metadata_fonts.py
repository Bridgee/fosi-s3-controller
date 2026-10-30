"""Rebuild Flash metadata fonts from pinned Noto, without partial output on failure."""
import argparse
from pathlib import Path
import tempfile
from font_generation import check_inputs, publish_assets, run_converter
from pack_metadata_fonts import pack_font

ROOT = Path(__file__).resolve().parents[1]
SHA256 = "68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5"
RANGES = "0x80-0x52f,0x2000-0x206f,0x3000-0x30ff,0x31f0-0x31ff,0x4e00-0x9fff,0xff00-0xffef"


def generate(root, font, converter, node):
    check_inputs(font, converter, node, SHA256, "1.5.3")
    outputs = {}
    with tempfile.TemporaryDirectory(prefix="orbit-font-convert-") as directory:
        for size in (24, 12):
            name = f"orbit_metadata_{size}.c"
            staged = Path(directory) / name
            run_converter([node, str(converter), "--font", str(font), "--range", RANGES,
                           "--size", str(size), "--bpp", "2", "--no-kerning", "--no-compress",
                           "--format", "lvgl", "--lv-include", "lvgl.h", "--lv-font-name",
                           f"orbit_metadata_{size}", "-o", str(staged)])
            text = staged.read_text().replace(str(font), "NotoSansCJKjp-Regular.otf").replace(str(staged), name)
            text = text.replace('#ifndef ORBIT_METADATA_', '#include "metadata_font.h"\n\n#ifndef ORBIT_METADATA_', 1)
            text = text.replace('= lv_font_get_glyph_dsc_fmt_txt,', '= orbit_metadata_get_glyph_dsc,')
            text = text.replace('= lv_font_get_bitmap_fmt_txt,', '= orbit_metadata_get_bitmap,')
            outputs[root / "firmware/OrbitS3" / name] = pack_font("/* Derived from Noto Sans CJK JP; SIL OFL 1.1. See assets/fonts/. */\n" + text)
    publish_assets(outputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True, help="lv_font_conv 1.5.3 CLI")
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    try:
        generate(ROOT, args.font, args.converter, args.node)
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + "\n")
    print("PASS: validated 24/12px metadata fonts generated; no runtime font download")


if __name__ == "__main__":
    main()
