"""Generate exact original48px numeric glyphs, kerning and line extents from pinned sources."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tempfile
from font_generation import c_array, check_inputs, field, integers, publish_assets, require, run_converter
from pack_metadata_fonts import ORIGINAL

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM_SHA256 = "f2c8ef9e9ffc81f72ff05f03344f744c3b808be6f32aeed6912ecd95242097a7"
CODEPOINTS = {0x20, *range(0x2b, 0x2f), *range(0x30, 0x3a), 0x43, 0x46, 0xb0}


def records(text):
    require(field(text, "bpp") == "4" and field(text, "bitmap_format") == "0", "Expected plain4bpp font")
    chunks = list(re.finditer(r"/\* U\+([0-9A-Fa-f]+) .*?\*/([\s\S]*?)(?=/\* U\+|$)", c_array(text, "glyph_bitmap")))
    descriptors = [tuple(map(int, row)) for row in re.findall(ORIGINAL, c_array(text, "glyph_dsc"))]
    require(len(chunks) + 1 == len(descriptors), "Glyph comments/descriptor count mismatch")
    result, offset = {}, 0
    for gid, chunk in enumerate(chunks, 1):
        cp = int(chunk[1], 16)
        desc, raw = descriptors[gid], integers(chunk[2])
        require(cp not in result, "Duplicate numeric codepoint")
        require(desc[0] == offset and len(raw) == (desc[2] * desc[3] * 4 + 7) // 8,
                "Numeric bitmap span/offset mismatch")
        result[cp] = (gid, desc[1:], chunk[2])
        offset += len(raw)
    return result


def kern_values(text, glyphs):
    require(field(text, "kern_classes") == "1", "Expected class-based numeric kerning")
    left, right = (integers(c_array(text, name)) for name in ("kern_left_class_mapping", "kern_right_class_mapping"))
    values = integers(c_array(text, "kern_class_values"))
    nl, nr = int(field(text, "left_class_cnt")), int(field(text, "right_class_cnt"))
    require(len(values) == nl * nr and max(row[0] for row in glyphs.values()) < min(len(left), len(right)), "Numeric kerning table size mismatch")
    require(all(0 <= n <= nl for n in left) and all(0 <= n <= nr for n in right), "Invalid numeric kerning class")
    scale = int(field(text, "kern_scale"))
    return {(a, b): (values[(left[ga[0]] - 1) * nr + right[gb[0]] - 1] * scale if left[ga[0]] and right[gb[0]] else 0)
            for a, ga in glyphs.items() for b, gb in glyphs.items()}


def preserve_subset(original, text):
    old, current = records(original), records(text)
    require(set(current) == CODEPOINTS and CODEPOINTS <= set(old), "Unexpected numeric subset coverage")
    require(all(current[cp][1] == old[cp][1] and len(integers(current[cp][2])) == len(integers(old[cp][2]))
                for cp in CODEPOINTS), "Numeric glyph metrics changed")
    old_selected = {cp: old[cp] for cp in CODEPOINTS}
    require(kern_values(original, old_selected) == kern_values(text, current), "Numeric pair kerning changed")
    start = text.index("glyph_bitmap[] = {") + len("glyph_bitmap[] = {")
    end = text.index("};", start)
    # Keep original bytes, including unused padding; do not accept new rasterization.
    pattern = r"(/\* U\+([0-9A-Fa-f]+) .*?\*/)([\s\S]*?)(?=/\* U\+|$)"
    body = re.sub(pattern, lambda match: match[1] + old[int(match[2], 16)][2], text[start:end])
    text = text[:start] + body + text[end:]
    for key in ("line_height", "base_line", "underline_position", "underline_thickness"):
        value = field(original, key)
        text = re.sub(r"(\." + key + r" = )-?\d+", lambda match: match[1] + value, text)
    text = text.replace("static lv_font_fmt_txt_dsc_t font_dsc", "static const lv_font_fmt_txt_dsc_t font_dsc").replace(".cache = &cache", ".cache = NULL")
    text = re.sub(r"static\s+lv_font_fmt_txt_glyph_cache_t cache;", "", text)
    # Keep generated C canonical and clean under git diff --check.
    return "\n".join(line.rstrip() for line in text.splitlines()).rstrip() + "\n"


def generate(root, font, converter, node):
    info = json.loads((root / "assets/fonts/montserrat-source.json").read_text())
    check_inputs(font, converter, node, info["sha256"], info["converterVersion"])
    upstream = root / ".pio/libdeps/orbit-live-check/lvgl/src/font/lv_font_montserrat_48.c"
    original = upstream.read_text()
    require(hashlib.sha256(original.encode()).hexdigest() == UPSTREAM_SHA256, "Pinned built-in font changed")
    name = "orbit_numeric_48.c"
    with tempfile.TemporaryDirectory(prefix="orbit-numeric-convert-") as directory:
        staged = Path(directory) / name
        run_converter([node, str(converter), "--font", str(font), "--range", "0x20,0x2b-0x2e,0x30-0x39,0x43,0x46,0xb0", "--size", "48", "--bpp", "4", "--no-compress", "--no-prefilter", "--force-fast-kern-format", "--format", "lvgl", "--lv-include", "lvgl.h", "--lv-font-name", "orbit_numeric_48", "-o", str(staged)])
        text = staged.read_text().replace(str(font), "Montserrat-Medium.ttf").replace(str(staged), name)
        result = preserve_subset(original, text)
    publish_assets({root / "firmware/OrbitS3" / name: "/* Orbit Numeric; derived from Montserrat 7.200, SIL OFL1.1. See assets/fonts/. */\n" + result})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    try:
        generate(ROOT, args.font, args.converter, args.node)
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + "\n")
    print("PASS: numeric coverage/metrics/kerning validated; exact original pixels and line extents retained")


if __name__ == "__main__":
    main()
