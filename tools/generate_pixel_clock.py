"""Original bitmap alphabet/numerals for the compact monochrome time tools.
All glyphs expand on integer pixel grids at build time; no font engine scaling.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
letters={
'A':['01110','10001','10001','11111','10001','10001','10001'],'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01111','10000','10000','10111','10001','10001','01111'],'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['111','010','010','010','010','010','111'],'J':['00111','00010','00010','00010','10010','10010','01100'],
'K':['10001','10010','10100','11000','10100','10010','10001'],'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],'N':['10001','11001','11001','10101','10011','10011','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],'P':['11110','10001','10001','11110','10000','10000','10000'],
'Q':['01110','10001','10001','10001','10101','10010','01101'],'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','11011','10001'],'X':['10001','10001','01010','00100','01010','10001','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],'Z':['11111','00001','00010','00100','01000','10000','11111'],
'0':['01110','10001','10001','10001','10001','10001','01110'],'1':['00100','01100','00100','00100','00100','00100','01110'],
'2':['01110','10001','00001','00010','00100','01000','11111'],'3':['11110','00001','00001','01110','00001','00001','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010'],'5':['11111','10000','10000','11110','00001','00001','11110'],
'6':['01110','10000','10000','11110','10001','10001','01110'],'7':['11111','00001','00010','00100','01000','01000','01000'],
'8':['01110','10001','10001','01110','10001','10001','01110'],'9':['01110','10001','10001','01111','00001','00001','01110'],
' ':['000']*7,':':['00','11','11','00','11','11','00'],'/':['00001','00001','00010','00100','01000','10000','10000'],
'-':['00000','00000','00000','11111','00000','00000','00000'],'+':['00000','00100','00100','11111','00100','00100','00000']}
big={
'0':['0111110','1111111','1100011','1100011','1100011','1100011','1100011','1100011','1111111','0111110'],
'1':['0001100','0011100','0111100','0001100','0001100','0001100','0001100','0001100','0111110','0111110'],
'2':['0111110','1111111','1100011','0000011','0000110','0001100','0011000','0110000','1111111','1111111'],
'3':['1111110','1111111','0000011','0000011','0011110','0011110','0000011','0000011','1111111','1111110'],
'4':['0000110','0001110','0011110','0110110','1100110','1100110','1111111','1111111','0000110','0000110'],
'5':['1111111','1111111','1100000','1100000','1111110','1111111','0000011','0000011','1111111','1111110'],
'6':['0011110','0111110','1100000','1100000','1111110','1111111','1100011','1100011','1111111','0111110'],
'7':['1111111','1111111','0000011','0000110','0001100','0011000','0011000','0110000','0110000','0110000'],
'8':['0111110','1111111','1100011','1100011','0111110','1111111','1100011','1100011','1111111','0111110'],
'9':['0111110','1111111','1100011','1100011','1111111','0111111','0000011','0000011','0111110','0111100'],
':':['00','00','11','11','00','00','11','11','00','00'],'-':['0000000']*4+['1111111']*2+['0000000']*4}
# Compact selector alphabet: six rows, still one physical 3px cell per bit.
compact={'A':['010','101','101','111','101','101'],'T':['111','010','010','010','010','010'],
'1':['010','110','010','010','010','111'],'2':['110','001','001','010','100','111'],
'3':['110','001','011','001','001','110'],'4':['101','101','111','001','001','001']}
c=['// Generated original integer-grid bitmap font.\n#include "pixel_clock_font.h"\n#include <string.h>\n']
PITCH=3
def snap(value):return int(((value+PITCH//2)//PITCH)*PITCH)
def extent(value):return ((value+2*PITCH-1)//(2*PITCH))*(2*PITCH)
fonts=[('orbit_pixel_18',compact,3),('orbit_pixel_21',letters,3),('orbit_pixel_28',letters,4),('orbit_pixel_40',big,4),('orbit_pixel_50',big,5),('orbit_pixel_100',big,10)]
for name,glyphs,scale in fonts:
 line_height=((len(next(iter(glyphs.values())))*scale+PITCH)//(2*PITCH))*(2*PITCH)
 raw=bytearray();entries=[]
 for ch,rows in sorted(glyphs.items()):
  # All font sizes address the SAME physical matrix, rather than inventing
  # a different pixel size for each glyph scale. Integer edges avoid AA/fringes.
  w=snap(len(rows[0])*scale);h=min(snap(len(rows)*scale),line_height)
  y_scale=min(scale,h/len(rows))
  raster=[['0']*w for _ in range(h)]
  for y,row in enumerate(rows):
   for x,value in enumerate(row):
    if value=='1':
     for py in range(snap(y*y_scale),snap((y+1)*y_scale)):
      for px in range(snap(x*scale),snap((x+1)*scale)):raster[py][px]='1'
  bits=''.join(''.join(row) for row in raster)
  start=len(raw);bits+='0'*((-len(bits))%8);raw.extend(int(bits[i:i+8],2) for i in range(0,len(bits),8));entries.append((ord(ch),start,w,h,((len(rows[0])+1)*scale+PITCH)//(2*PITCH)*(2*PITCH)))
 c.append('static const uint8_t '+name+'_pixels[]={'+','.join(str(b) for b in raw)+'};\n')
 c.append('static const Glyph '+name+'_glyphs[]={'+','.join('{%d,%d,%d,%d,%d}'%e for e in entries)+'};\n')
 c.append('static const Data '+name+'_data={'+name+'_pixels,'+name+'_glyphs,'+str(len(entries))+'};\n')
 c.append('const lv_font_t '+name+'={glyph_info,glyph_bitmap,'+str(line_height)+',0,LV_FONT_SUBPX_NONE,0,0,&'+name+'_data,nullptr,nullptr};\n')
# Public font callbacks share fixed Flash data; exact1-bit glyphs, no antialiasing.
header='''struct Glyph {uint32_t code,offset;uint16_t width,height,advance;};
struct Data {const uint8_t *pixels;const Glyph *glyphs;unsigned count;};
static const Glyph *find(const lv_font_t *font,uint32_t code){const Data *d=static_cast<const Data*>(font->dsc);for(unsigned i=0;i<d->count;++i)if(d->glyphs[i].code==code)return &d->glyphs[i];return nullptr;}
static bool glyph_info(const lv_font_t *font,lv_font_glyph_dsc_t *out,uint32_t code,uint32_t){auto *g=find(font,code);if(!g)return false;memset(out,0,sizeof(*out));out->adv_w=g->advance;out->box_w=g->width;out->box_h=g->height;out->bpp=1;return true;}
static const uint8_t *glyph_bitmap(const lv_font_t *font,uint32_t code){auto *g=find(font,code);return g?static_cast<const Data*>(font->dsc)->pixels+g->offset:nullptr;}
'''
c.insert(1,header)
(r/'firmware/OrbitS3/pixel_clock_font.cpp').write_text(''.join(c))
(r/'firmware/OrbitS3/pixel_clock_font.h').write_text('#pragma once\n#include <lvgl.h>\n'+''.join('extern const lv_font_t '+name+';\n' for name,_,_ in fonts)+'''namespace orbit_pixel_grid {
constexpr int Pitch=3;
inline int snap(int v){return v<0?-((-v+Pitch/2)/Pitch)*Pitch:((v+Pitch/2)/Pitch)*Pitch;}
inline int extent(int v){return ((v+2*Pitch-1)/(2*Pitch))*(2*Pitch);}
}
''')
# Browser uses original SVG path glyphs, precise dimensions and same integer scale.
# The symbol source is shared rather than relying on a host-installed pixel font.
js='window.orbitPixelGlyphs='+__import__('json').dumps({'small':letters,'large':big,'compact':compact},separators=(',',':'))+';\n'
js+='''// Shared 3px display matrix; advance and line boxes use even cell counts.
window.orbitPixelGrid={pitch:3,snap:v=>Math.round(v/3)*3,extent:v=>Math.ceil(v/6)*6};
'''
d=r/'assets/clock';(d/'pixel-glyphs.js').write_text(js)
print('PASS: original integer-grid clock fonts generated')
