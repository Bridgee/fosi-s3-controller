"""Offline font tool negative/staging tests; synthetic assets, no converter download/device."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import font_generation as shared
import generate_metadata_fonts as metadata
import generate_latin_fonts as latin
import generate_numeric_font as numeric
from pack_metadata_fonts import pack_font, validate_packed

ROOT=Path(__file__).resolve().parents[1]
RAW="""#include "metadata_font.h"
static const uint8_t glyph_bitmap[] = {
    0xc0
};
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 16, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0}
};
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {.range_start = 65, .range_length = 1, .glyph_id_start = 1,
     .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}
};
#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif
static const lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap, .glyph_dsc = glyph_dsc, .cmaps = cmaps,
    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = 1, .bpp = 2,
    .kern_classes = 0, .bitmap_format = 0,
    .cache = &cache
};
/*-----------------
 *  PUBLIC FONT
 *----------------*/
const lv_font_t sample = {.get_glyph_dsc = orbit_metadata_get_glyph_dsc,
    .get_glyph_bitmap = orbit_metadata_get_bitmap, .fallback = NULL};
"""


class FontToolTests(unittest.TestCase):
    def test_idempotent_and_actual_assets(self):
        packed=pack_font(RAW)
        self.assertEqual(pack_font(packed),packed)
        for path in (ROOT/"firmware/OrbitS3").glob("orbit_metadata_*.c"):
            self.assertEqual(pack_font(path.read_text()),path.read_text())

    def test_bad_original_format_metrics_bitmap(self):
        bad=[RAW.replace('.bpp = 2','.bpp = 8'),
             RAW.replace('orbit_metadata_get_glyph_dsc','lv_font_get_glyph_dsc_fmt_txt'),
             RAW.replace('.kern_dsc = NULL','.kern_dsc = &unexpected'),
             RAW.replace('.adv_w = 16','.adv_w = 65536'),
             RAW.replace('.ofs_x = 0','.ofs_x = -129'),
             RAW.replace('.box_w = 1','.box_w = 256'),
             RAW.replace('.box_h = 1','.box_h = 9'),
             RAW.replace('.glyph_id_start = 1','.glyph_id_start = 2'),
             RAW.replace('.range_length = 1','.range_length = 0'),
             RAW.replace('LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY','UNKNOWN_CMAP'),
             RAW.replace('.cmap_num = 1','.cmap_num = 2')]
        for text in bad:
            with self.subTest(text=text[-50:]), self.assertRaises(ValueError):
                pack_font(text)

    def test_corrupt_packed_assets_are_revalidated(self):
        packed=pack_font(RAW)
        bad=[packed.replace('.glyph_count = 2','.glyph_count = 100'),
             packed.replace('orbit_metadata_font_dsc_t font_dsc','lv_font_fmt_txt_dsc_t font_dsc'),
             packed.replace('orbit_metadata_get_bitmap','lv_font_get_bitmap_fmt_txt'),
             packed.replace('glyph_offsets[] = {\n    0, 0,','glyph_offsets[] = {\n    0, 2,'),
             packed.replace('.box_h = 1','.box_h = 255'),
             packed.replace('.bitmap_size = sizeof glyph_bitmap','.bitmap_size = 100'),
             packed.replace('.cmap_num = 1','.cmap_num = 9')]
        for text in bad:
            with self.subTest(), self.assertRaises(ValueError):
                validate_packed(text)

    def test_sparse_and_dense_list_validation(self):
        for kind, ids in [('LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL','uint8_t'),
                          ('LV_FONT_FMT_TXT_CMAP_SPARSE_FULL','uint16_t')]:
            text=RAW.replace('LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY',kind).replace('.glyph_id_ofs_list = NULL','.glyph_id_ofs_list = ids').replace('.list_length = 0','.list_length = 1')
            text='static const '+ids+' ids[] = {\n 0\n};\n'+text
            if 'SPARSE' in kind:
                text='static const uint16_t unicode[] = {\n 0\n};\n'+text.replace('.unicode_list = NULL','.unicode_list = unicode')
            pack_font(text)
            for bad in [text.replace('.list_length = 1','.list_length = 2'),text.replace('ids[] = {\n 0','ids[] = {\n 3')]:
                with self.assertRaises(ValueError):pack_font(bad)
        sparse='static const uint16_t unicode[] = {\n 0, 0\n};\n'+RAW.replace('LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY','LV_FONT_FMT_TXT_CMAP_SPARSE_TINY').replace('.unicode_list = NULL','.unicode_list = unicode').replace('.list_length = 0','.list_length = 2')
        with self.assertRaisesRegex(ValueError,'sparse unicode'):pack_font(sparse)

    def test_checks_survive_optimized_python(self):
        code="import sys; sys.path.insert(0,sys.argv[1]); from font_generation import require; require(False,'guard still active')"
        result=subprocess.run([sys.executable,'-O','-c',code,str(ROOT/'tools')],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('guard still active',result.stderr)

    def test_source_and_version_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            font=Path(directory)/'source';font.write_bytes(b'not a real source')
            with patch.object(shared,'run_converter') as run:
                with self.assertRaisesRegex(ValueError,'checksum'):
                    shared.check_inputs(font,Path('converter'),'node','wrong','1.5.3')
                run.assert_not_called()
                run.return_value='wrong-version'
                with self.assertRaisesRegex(ValueError,'version'):
                    shared.check_inputs(font,Path('converter'),'node',hashlib.sha256(font.read_bytes()).hexdigest(),'1.5.3')

    def test_converter_deadlines_and_errors(self):
        with patch.object(shared.subprocess,'run',side_effect=subprocess.TimeoutExpired('fake',120)):
            with self.assertRaisesRegex(ValueError,'timed out'):shared.run_converter(['fake'])
        with patch.object(shared.subprocess,'run',side_effect=subprocess.CalledProcessError(1,'fake',stderr='synthetic failure')):
            with self.assertRaisesRegex(ValueError,'synthetic failure'):shared.run_converter(['fake'])

    def test_publish_failure_rolls_back_and_cleans_stage(self):
        for initial in (True,False):
            with tempfile.TemporaryDirectory() as directory:
                a,b=Path(directory)/'a.c',Path(directory)/'b.c'
                if initial:a.write_text('old-a')
                b.write_text('old-b')
                original_replace=shared.os.replace;count=0
                def fail_second(source,dest):
                    nonlocal count
                    count+=1
                    if count==2:raise OSError('synthetic second rename failure')
                    return original_replace(source,dest)
                with patch.object(shared.os,'replace',side_effect=fail_second):
                    with self.assertRaises(OSError):shared.publish_assets({a:'new-a',b:'new-b'})
                self.assertEqual(a.exists(),initial)
                if initial:self.assertEqual(a.read_text(),'old-a')
                self.assertEqual(b.read_text(),'old-b')
                self.assertFalse(list(Path(directory).glob('.font-stage-*')))

    def test_generators_do_not_publish_partial_set(self):
        for module,names in [(metadata,['orbit_metadata_24.c','orbit_metadata_12.c']),
                             (latin,['orbit_metadata_latin_24.c','orbit_metadata_latin_14.c','metadata_latin_metrics.h'])]:
            with tempfile.TemporaryDirectory() as directory:
                root=Path(directory);dest=root/'firmware/OrbitS3';dest.mkdir(parents=True)
                info=root/'assets/fonts';info.mkdir(parents=True)
                shutil.copyfile(ROOT/'assets/fonts/montserrat-source.json',info/'montserrat-source.json')
                for name in names:(dest/name).write_text('prior-'+name)
                count=0
                def fake_convert(command):
                    nonlocal count
                    count+=1
                    if count==2:raise ValueError('synthetic second conversion failure')
                    Path(command[command.index('-o')+1]).write_text(RAW)
                with patch.object(module,'check_inputs'),patch.object(module,'run_converter',side_effect=fake_convert):
                    with self.assertRaises(ValueError):module.generate(root,Path('font'),Path('converter'),'node')
                for name in names:self.assertEqual((dest/name).read_text(),'prior-'+name)

    def test_numeric_metrics_coverage_and_kerning_guards(self):
        original=(ROOT/'.pio/libdeps/orbit-live-check/lvgl/src/font/lv_font_montserrat_48.c').read_text()
        current=(ROOT/'firmware/OrbitS3/orbit_numeric_48.c').read_text()
        self.assertEqual(numeric.preserve_subset(original,current),current)
        self.assertTrue(current.endswith("\n") and not current.endswith("\n\n"))
        self.assertTrue(all(line == line.rstrip() for line in current.splitlines()))
        adv=numeric.records(current)[ord('C')][1][0]
        bad=[current.replace('U+0043','U+0044'),
             current.replace('.adv_w = '+str(adv)+',','.adv_w = '+str(adv+1)+',',1),
             current.replace('.kern_scale = 16','.kern_scale = 15')]
        for text in bad:
            with self.subTest(),self.assertRaises(ValueError):numeric.preserve_subset(original,text)

    def test_numeric_invalid_output_retains_old_asset(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);dest=root/'firmware/OrbitS3';dest.mkdir(parents=True)
            old=dest/'orbit_numeric_48.c';old.write_text('prior numeric asset')
            info=root/'assets/fonts';info.mkdir(parents=True)
            shutil.copyfile(ROOT/'assets/fonts/montserrat-source.json',info/'montserrat-source.json')
            upstream=root/'.pio/libdeps/orbit-live-check/lvgl/src/font';upstream.mkdir(parents=True)
            shutil.copyfile(ROOT/'.pio/libdeps/orbit-live-check/lvgl/src/font/lv_font_montserrat_48.c',upstream/'lv_font_montserrat_48.c')
            def invalid(command):Path(command[command.index('-o')+1]).write_text('invalid generated font')
            with patch.object(numeric,'check_inputs'),patch.object(numeric,'run_converter',side_effect=invalid):
                with self.assertRaises(ValueError):numeric.generate(root,Path('font'),Path('converter'),'node')
            self.assertEqual(old.read_text(),'prior numeric asset')

    def test_missing_golden_fails_early_even_optimized(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'tools').mkdir()
            target=root/'tools/test_metadata_fonts.py'
            shutil.copyfile(ROOT/'tools/test_metadata_fonts.py',target)
            result=subprocess.run([sys.executable,'-O',str(target)],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('Required original font baseline is missing',result.stderr)


if __name__=='__main__':
    unittest.main()
