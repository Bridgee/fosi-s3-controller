"""Lossless aligned metadata storage, strict bitmap/cmap validation; no runtime compression."""
import re
from pathlib import Path
from font_generation import c_array, field, integers, publish_assets, require

METRICS = r"\{\.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}"
ORIGINAL = r"\{\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}"


def validate_tables(text, offsets, metrics):
    raw = integers(c_array(text, "glyph_bitmap"))
    require(all(0 <= value <= 255 for value in raw), "Invalid bitmap byte")
    bpp = int(field(text, "bpp"))
    require(bpp in (2, 4), "Only plain 2/4 bpp metadata fonts are supported")
    require(len(offsets) == len(metrics) and bool(metrics), "Mismatched glyph tables")
    for off, (adv, w, h, x, y) in zip(offsets, metrics):
        require(0 <= off <= 0xffffffff and 0 <= adv <= 65535 and
                0 <= w <= 255 and 0 <= h <= 255 and -128 <= x <= 127 and -128 <= y <= 127,
                "Glyph does not fit lossless metadata storage")
        size = (w * h * bpp + 7) // 8
        require(off <= len(raw) and size <= len(raw) - off, "Glyph bitmap exceeds array bounds")
    maps = re.findall(r"\{([^{}]*)\}", c_array(text, "cmaps"))
    require(len(maps) == int(field(text, "cmap_num")), "Incorrect cmap count")
    previous_end = 0
    for entry in maps:
        start, length, first = (int(field(entry, name), 0) for name in
                                ("range_start", "range_length", "glyph_id_start"))
        require(0 < length <= 65535 and previous_end <= start and start + length <= 0x110000,
                "Unsorted, overlapping or invalid cmap range")
        previous_end = start + length
        kind = field(entry, "type")
        list_length = int(field(entry, "list_length"))
        unicode_name, ids_name = field(entry, "unicode_list"), field(entry, "glyph_id_ofs_list")
        if kind in ("LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY", "LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL"):
            require(unicode_name == "NULL", "Unexpected dense unicode list")
            if kind.endswith("_TINY"):
                require(ids_name == "NULL" and list_length == 0, "Unexpected tiny cmap offsets")
                ids = range(length)
            else:
                ids = integers(c_array(text, ids_name))
                require(list_length == length and len(ids) == length and all(0 <= n <= 255 for n in ids), "Invalid dense offsets")
        elif kind in ("LV_FONT_FMT_TXT_CMAP_SPARSE_TINY", "LV_FONT_FMT_TXT_CMAP_SPARSE_FULL"):
            codepoints = integers(c_array(text, unicode_name))
            require(len(codepoints) == list_length and all(0 <= n < length for n in codepoints) and
                    all(a < b for a, b in zip(codepoints, codepoints[1:])), "Invalid sparse unicode list")
            if kind.endswith("_TINY"):
                require(ids_name == "NULL", "Unexpected sparse tiny offsets")
                ids = range(list_length)
            else:
                ids = integers(c_array(text, ids_name))
                require(len(ids) == list_length and all(0 <= n <= 65535 for n in ids), "Invalid sparse offsets")
        else:
            raise ValueError("Unsupported cmap type: " + kind)
        require(first > 0 and all(first + n < len(metrics) for n in ids), "Cmap glyph ID exceeds table")


def validate_packed(text):
    require('static const orbit_metadata_font_dsc_t font_dsc' in text,
            "Packed metadata descriptor ABI is incorrect")
    require(field(text, "get_glyph_dsc") == "orbit_metadata_get_glyph_dsc" and
            field(text, "get_glyph_bitmap") == "orbit_metadata_get_bitmap",
            "Packed metadata must use custom callbacks")
    block = c_array(text, "glyph_metrics")
    metrics = [tuple(map(int, row)) for row in re.findall(METRICS, block)]
    require(len(metrics) == block.count(".adv_w"), "Unexpected packed metrics format")
    offsets = integers(c_array(text, "glyph_offsets"))
    require(len(metrics) == int(field(text, "glyph_count")), "Incorrect glyph count")
    require(field(text, "bitmap_size") == "sizeof glyph_bitmap", "Missing bitmap size")
    validate_tables(text, offsets, metrics)


def pack_font(text):
    require('#include "metadata_font.h"' in text and
            field(text, "get_glyph_dsc") == "orbit_metadata_get_glyph_dsc" and
            field(text, "get_glyph_bitmap") == "orbit_metadata_get_bitmap",
            "Use metadata generators to set the header and custom callbacks before packing")
    if "orbit_metadata_metrics_t glyph_metrics" in text:
        # Upgrade older packed assets, then validate even the idempotent path.
        if ".bitmap_size" not in text:
            text = text.replace(".glyph_bitmap = glyph_bitmap,", ".glyph_bitmap = glyph_bitmap,\n    .bitmap_size = sizeof glyph_bitmap,", 1)
        validate_packed(text)
        return text
    match = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\n\};", text, re.S)
    require(match is not None, "Missing original metadata descriptors")
    entries = [tuple(map(int, row)) for row in re.findall(ORIGINAL, match[1])]
    require(len(entries) == match[1].count(".bitmap_index") and bool(entries), "Unexpected glyph descriptor format")
    validate_tables(text, [e[0] for e in entries], [e[1:] for e in entries])
    metrics = re.sub(ORIGINAL, lambda m: "{.adv_w = %s, .box_w = %s, .box_h = %s, .ofs_x = %s, .ofs_y = %s}" % m.groups()[1:], match[1])
    offsets = "\n".join("    " + ", ".join(str(e[0]) for e in entries[i:i+12]) + "," for i in range(0, len(entries), 12))
    replacement = "static const uint32_t glyph_offsets[] = {\n" + offsets + "\n};\n\nstatic const orbit_metadata_metrics_t glyph_metrics[] = {" + metrics + "\n};"
    text = text[:match.start()] + replacement + text[match.end():]
    marker = "#if LVGL_VERSION_MAJOR == 8\n/*Store all the custom data of the font*/"
    require(marker in text, "Unsupported converter descriptor block")
    start = text.index(marker)
    end = text.index("/*-----------------\n *  PUBLIC FONT", start)
    block = text[start:end]
    require(field(block, "kern_dsc") == "NULL" and all(field(block, name) == "0" for name in
            ("kern_scale", "kern_classes", "bitmap_format")), "Only plain no-kerning metadata fonts are supported")
    descriptor = """/* Flash-only descriptors, read exclusively by metadata_font.c callbacks. */
static const orbit_metadata_font_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .bitmap_size = sizeof glyph_bitmap,
    .glyph_offsets = glyph_offsets,
    .glyph_metrics = glyph_metrics,
    .cmaps = cmaps,
    .glyph_count = %d,
    .cmap_num = %s,
    .bpp = %s
};


""" % (len(entries), field(block, "cmap_num"), field(block, "bpp"))
    result = text[:start] + descriptor + text[end:]
    validate_packed(result)
    return result


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    outputs = {path: pack_font(path.read_text()) for path in sorted(root.glob("firmware/OrbitS3/orbit_metadata_*.c"))}
    publish_assets(outputs)
    print("PASS: all metadata assets validated and packed without changing glyphs")
