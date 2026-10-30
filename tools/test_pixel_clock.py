"""Verify fixed physical clock cells, static glyph budgets and optional LVGL output."""
from pathlib import Path
import argparse,re

ROOT=Path(__file__).resolve().parents[1]
def check_fonts(root=ROOT):
    text=(root/'firmware/OrbitS3/pixel_clock_font.cpp').read_text()
    total=count=0
    for size in (18,21,28,40,50,100):
        name='orbit_pixel_'+str(size)
        raw=bytes(map(int,re.search(r'const uint8_t '+name+r'_pixels\[\]=\{([^}]+)\}',text)[1].split(',')))
        total+=len(raw)
        glyphs=re.search(r'const Glyph '+name+r'_glyphs\[\]=\{(.+?)\};',text)[1]
        line=int(re.search(r'const lv_font_t '+name+r'=\{glyph_info,glyph_bitmap,(\d+)',text)[1])
        assert line%6==0
        for code,offset,w,h,advance in re.findall(r'\{(\d+),(\d+),(\d+),(\d+),(\d+)\}',glyphs):
            code,offset,w,h,advance=map(int,(code,offset,w,h,advance))
            assert w%3==h%3==0 and advance%6==0 and advance>=w and h<=line
            def bit(x,y):
                n=y*w+x;return (raw[offset+n//8]>>(7-n%8))&1
            for y in range(0,h,3):
                for x in range(0,w,3):assert len({bit(x+i,y+j) for j in range(3) for i in range(3)})==1,(size,code,x,y)
            count+=1
    assert total<25000,'Clock glyph budget unexpectedly grew'
    print(f'PASS: {count} static glyphs: shared 3px cells, aligned advance/line boxes, no partial cells; {total:,} bitmap bytes in Flash, no runtime glyph buffers')

def check_render(path):
    from PIL import Image
    image=Image.open(path).convert('RGB');assert image.size==(480,480)
    pixels=image.load();patterns={};bad=[]
    for y in range(0,480,3):
        for x in range(0,480,3):
            tile=tuple(pixels[x+i,y+j] for j in range(3) for i in range(3))
            on=[any(c) for c in tile]
            if any(on):
                patterns[tile]=patterns.get(tile,0)+1
                if not all(on):bad.append((x,y))
    assert not bad,('Partially lit cells in actual LVGL raster',len(bad),bad[:20])
    assert len(patterns)==1,('Uneven seam phase or rendering',len(patterns))
    tile=next(iter(patterns));assert tile[0][0]>tile[2][0]>tile[8][0]>0
    print(f'PASS: actual LVGL raster: {sum(patterns.values())} whole lit cells; all use the same subtle seam pattern, black cells stay black')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--render',type=Path);args=parser.parse_args()
    check_fonts()
    if args.render:check_render(args.render)
