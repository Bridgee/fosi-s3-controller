"""Synchronize both offline previews from committed firmware UI/assets.
--check is dependency-free and read-only; generation uses already-pinned LVGL.
No network, private configuration, device or API-key access.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[1]
PAGES=('index.html','orbit-s3-preview.html')
INPUTS=('firmware/OrbitS3/firmware_version.h','firmware/OrbitS3/orbit_ui.cpp','firmware/OrbitS3/button_layout.h','firmware/OrbitS3/ui_themes.h','firmware/OrbitS3/status_icons.cpp','firmware/OrbitS3/source_icons.cpp','firmware/OrbitS3/status_feedback.h','firmware/OrbitS3/metadata_font.c','firmware/OrbitS3/metadata_latin_metrics.h','firmware/OrbitS3/weather_labels.cpp','firmware/OrbitS3/weather_assets.cpp','firmware/OrbitS3/lyrics_icon.h','firmware/OrbitS3/swipe_gesture.h','firmware/OrbitS3/touch_regions.h','assets/themes.json','assets/lyrics/preview-data.json','assets/lyrics/preview-model.js','assets/lyrics/preview-ui.js','assets/screen-idle/preview.js','firmware/OrbitS3/lyrics.h','firmware/OrbitS3/lyrics_parser.cpp','firmware/OrbitS3/lyrics_protocol.h','firmware/OrbitS3/lyrics.cpp','assets/preview/layout.css','assets/preview/runtime.js','assets/preview/embed-frame.css','assets/clock/pixel-glyphs.js','assets/timezones/rules.js','assets/weather/catalog.js','assets/source-icons/protocols.json','assets/source-icons/symbols.html','assets/fonts/montserrat-source.json','ai/mock/preview.js','tools/sync_preview.py','tools/sync_source_icons.py','firmware/OrbitS3/source_status.h','firmware/OrbitS3/status_identity.h')

def script_json(value):
 return json.dumps(value,ensure_ascii=False,separators=(',',':')).replace('&','\\u0026').replace('<','\\u003c').replace('>','\\u003e').replace('\u2028','\\u2028').replace('\u2029','\\u2029')

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def version():return re.search(r'ORBIT_FIRMWARE_VERSION "([^"]+)"',(ROOT/INPUTS[0]).read_text())[1]
def rgba_svg(alpha,width,height):
 paths=[]
 for y in range(height):
  x=0
  while x<width:
   opacity=alpha[y*width+x]
   if not opacity:x+=1;continue
   end=x+1
   while end<width and alpha[y*width+end]==opacity:end+=1
   paths.append(f'<path fill="white" opacity="{opacity/255:.6f}" d="M{x} {y}h{end-x}v1H{x}Z"/>');x=end
 return f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">'+''.join(paths)+'</svg>'

def scoped_css(css, page):
 if page=='index.html':return css
 # This canonical sheet deliberately has flat selectors and one @font-face.
 # Scope every comma-separated selector, never declarations or font-face.
 def scope(match):
  prefix,selectors,body=match.groups()
  if selectors.lstrip().startswith('@'):return match[0]
  return prefix+','.join(('.device-only #orbit-s3-preview '+s.strip()[13:]) if s.strip().startswith('.device-only ') else '#orbit-s3-preview '+s.strip() for s in selectors.split(','))+'{'+body+'}'
 return re.sub(r'(^|\n)([^{}\n]+)\{([^{}]*)\}',scope,css)

def screen_block(text):
 start=re.search(r'<div class="screen" id="screen">',text).start();depth=0
 for tag in re.finditer(r'</?div\b[^>]*>',text[start:]):
  depth+=-1 if tag[0].startswith('</') else 1
  if depth==0:return start,start+tag.end()
 raise AssertionError('Unbalanced screen markup')

def main_script(text):return re.search(r'<script>([\s\S]*?)</script>',text)[1]

def style_block(text):return re.search(r'<style>([\s\S]*?)</style>',text)[1]

def scope_stylesheet(css):
 # Scope the complete canonical stylesheet, including nested media rules.
 # Keep keyframe steps and font descriptors intact. No CSS dependencies needed.
 def selector(value):
  value=value.strip()
  if value.startswith('html'):return value
  if value.startswith('.device-only '):return '.device-only #orbit-s3-preview '+value[13:]
  if value in (':root','body','*'):return '#orbit-s3-preview'+(' *' if value=='*' else '')
  return '#orbit-s3-preview '+value
 output='';position=0
 while position<len(css):
  opening=css.find('{',position)
  if opening<0:return output+css[position:]
  raw_head=css[position:opening];comments='\n'.join(re.findall(r'/\*[\s\S]*?\*/',raw_head));head=re.sub(r'/\*[\s\S]*?\*/','',raw_head).strip();depth=1;end=opening+1;quote='';escaped=False
  while end<len(css) and depth:
   char=css[end]
   if quote:
    if escaped:escaped=False
    elif char=='\\':escaped=True
    elif char==quote:quote=''
   elif char in ('"',"'"):quote=char
   elif char=='{':depth+=1
   elif char=='}':depth-=1
   end+=1
  assert depth==0,'Unbalanced preview stylesheet'
  body=css[opening+1:end-1]
  if head.startswith(('@media','@supports','@container')):body=scope_stylesheet(body)
  elif not head.startswith('@'):
   selectors=[];part='';nest=0
   for char in head+',':
    if char in '([':nest+=1
    elif char in ')]':nest-=1
    if char==',' and nest==0:selectors.append(selector(part));part=''
    else:part+=char
   head=','.join(selectors)
  output+=(comments+'\n' if comments else '')+head+'{'+body+'}\n';position=end
 return output

def alternate_style(canonical):
 base=re.sub(r'/\* orbit-preview-parity:start \*/[\s\S]*?/\* orbit-preview-parity:end \*/','',style_block(canonical))
 return scope_stylesheet(base)+'\n'+(ROOT/'assets/preview/embed-frame.css').read_text()+'\n/* orbit-preview-parity:start */\n'+scoped_css((ROOT/'assets/preview/layout.css').read_text(),'orbit-s3-preview.html')+'\n/* orbit-preview-parity:end */\n'

def check():
 path=ROOT/'assets/preview/manifest.json'
 assert path.is_file(),'Preview contract missing: run python3 tools/sync_preview.py'
 contract=json.loads(path.read_text())
 assert set(contract['inputs'])==set(INPUTS),'Preview input list is stale; run tools/sync_preview.py'
 assert contract['firmwareVersion']==version(),'Preview firmware version is stale; run tools/sync_preview.py'
 for name,value in contract['inputs'].items():assert digest(ROOT/name)==value,'Preview input changed: '+name+'; run tools/sync_preview.py'
 for name,value in contract['assets'].items():assert digest(ROOT/name)==value,'Preview asset changed: '+name
 blocks=[]
 for name in PAGES:
  text=(ROOT/name).read_text()
  block=re.search(r'// orbit-preview-parity:start([\s\S]*?)// orbit-preview-parity:end',text)
  assert block and f'"firmwareVersion":"{version()}"' in block[1],name+' missing current contract'
  assert 'id="aboutFirmware"' in text
  for asset in ('assets/lyrics/preview-model.js','assets/lyrics/preview-ui.js'):assert (ROOT/asset).read_text() in text,name+' stale lyric model/UI'
  lyric_data=json.loads((ROOT/'assets/lyrics/preview-data.json').read_text())
  assert 'const lyricsPreviewCatalog='+script_json(lyric_data)+';' in text,name+' stale lyric fixtures'
  assert not re.search(r'<dt>Firmware</dt><dd[^>]*>\d+\.\d+\.\d+</dd>',text),'Hardcoded browser firmware version'
  assert '0.1.18' not in text,name+' obsolete version'
  assert scoped_css((ROOT/'assets/preview/layout.css').read_text(),name) in text
  assert (ROOT/'assets/preview/runtime.js').read_text() in block[1]
  blocks.append(block[1])
 assert blocks[0]==blocks[1],'Standalone previews have divergent parity logic'
 pages=[(ROOT/name).read_text() for name in PAGES]
 assert style_block(pages[1])==alternate_style(pages[0]),'Preview styles diverged; run tools/sync_preview.py'
 assert main_script(pages[0])==main_script(pages[1]),'Preview application logic diverged; run tools/sync_preview.py'
 a,b=screen_block(pages[0]);c,d=screen_block(pages[1])
 for id in ('launchAppleMusic','artworkBackdrop','artworkLarge','deviceStatus'):
  assert 'id="'+id+'"' in pages[0][a:b],'Screen excludes '+id
 assert pages[0][a:b]==pages[1][c:d],'Preview screen markup diverged; run tools/sync_preview.py'
 return contract

def generate():
 out=ROOT/'assets/preview';out.mkdir(exist_ok=True)
 library=ROOT/'.pio/libdeps/orbit-live-check/lvgl'
 font=library/'scripts/built_in_font/Montserrat-Medium.ttf'
 expected=json.loads((ROOT/'assets/fonts/montserrat-source.json').read_text())['sha256']
 assert digest(font)==expected,'Pinned Montserrat source mismatch'
 shutil.copyfile(font,out/'Montserrat-Medium.ttf')
 generated=[out/'Montserrat-Medium.ttf'];glyphs={}
 for size in (18,24,26):
  text=(library/f'src/font/lv_font_montserrat_{size}.c').read_text()
  bitmaps=text.split('glyph_bitmap[] = {',1)[1].split('};',1)[0]
  chunks=list(re.finditer(r'/\* U\+([0-9A-F]+) .*?\*/([\s\S]*?)(?=/\* U\+|$)',bitmaps))
  descriptors=re.findall(r'\{\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}',text)
  assert len(descriptors)==len(chunks)+1,(size,len(descriptors),len(chunks))
  lh=int(re.search(r'\.line_height = (\d+)',text)[1]);baseline=int(re.search(r'\.base_line = (\d+)',text)[1])
  wanted={'volume':0xf027,'mute':0xf026,'power':0xf011,'back':0xf053,'theme':0xf043,'bluetooth':0xf293,'play':0xf04b,'pause':0xf04c,'prev':0xf048,'next':0xf051}
  for name,cp in wanted.items():
   index=next((i for i,m in enumerate(chunks) if int(m[1],16)==cp),None)
   if index is None:continue
   _,adv,w,h,ox,oy=map(int,descriptors[index+1]);raw=bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]+)',chunks[index][2]));assert len(raw)==(w*h+1)//2
   width=max(size,(adv+8)//16);alpha=[0]*(width*lh)
   left=(width-(adv+8)//16)//2+ox;top=lh-baseline-h-oy
   for y in range(h):
    for x in range(w):
     at=y*w+x;value=((raw[at//2]>>(0 if at%2 else 4))&15)*17
     if 0<=left+x<width and 0<=top+y<lh:alpha[(top+y)*width+left+x]=value
   path=out/f'{name}-{size}.svg';svg=rgba_svg(alpha,width,lh);path.write_text(svg);generated.append(path);glyphs[f'{name}{size}']={'width':width,'height':lh,'path':str(path.relative_to(ROOT))}
   if name=='mute' and size==26:
    path=out/'mute-26-slash.svg';x=width/2+1;y=lh/2+1
    path.write_text(svg.replace('</svg>',f'<path d="M{x-9} {y+9}L{x+9} {y-9}" fill="none" stroke="white" stroke-width="2" stroke-linecap="round"/></svg>'));generated.append(path)
 statuses=(ROOT/'firmware/OrbitS3/status_icons.cpp').read_text()
 for name,values in re.findall(r'pixels_(\w+)\[576\]=\{([\s\S]*?)\};',statuses):
  alpha=list(map(int,re.findall(r'\d+',values)));assert len(alpha)==576
  path=out/f'status-{name}.svg';path.write_text(rgba_svg(alpha,24,24));generated.append(path)
 # A local native encoder creates the same deterministic sample QR as the screen.
 qrdir=library/'src/extra/libs/qrcode'
 urls=['https://example.org/local-introduction']+[f'https://example.org/reference/{i}' for i in (2,3,4)]
 qr_sources={}
 with tempfile.TemporaryDirectory(prefix='fosi-preview-qr-') as tmp:
  tmp=Path(tmp);(tmp/'lv_conf.h').write_text('#define LV_USE_LOG 0\n#define LV_USE_ASSERT_NULL 0\n#define LV_MEM_CUSTOM 1\n')
  (tmp/'qr.c').write_text('#include "qrcodegen.h"\n#include <stdio.h>\nint main(int n,char **v){unsigned char a[qrcodegen_BUFFER_LEN_FOR_VERSION(16)],b[qrcodegen_BUFFER_LEN_FOR_VERSION(16)];if(n!=2||!qrcodegen_encodeText(v[1],a,b,qrcodegen_Ecc_MEDIUM,1,16,qrcodegen_Mask_AUTO,1))return 1;int s=qrcodegen_getSize(b);printf("%d\\n",s);for(int y=0;y<s;y++)for(int x=0;x<s;x++)putchar(qrcodegen_getModule(b,x,y)?\'1\':\'0\');return 0;}')
  binary=tmp/'qr';subprocess.run(['cc','-DLV_CONF_INCLUDE_SIMPLE','-DLV_CONF_SUPPRESS_DEFINE_CHECK','-I'+str(tmp),'-I'+str(qrdir),str(tmp/'qr.c'),str(qrdir/'qrcodegen.c'),'-o',str(binary)],check=True,capture_output=True)
  for index,url in enumerate(urls):
   result=subprocess.check_output([str(binary),url],text=True);size,modules=result.split('\n');size=int(size);scale=200//(size+8);side=(size+8)*scale;offset=(200-side)//2
   parts=[f'<rect x="{offset}" y="{offset}" width="{side}" height="{side}" fill="white"/>']
   for y in range(size):
    for x in range(size):
     if modules[y*size+x]=='1':parts.append(f'<path d="M{offset+(x+4)*scale} {offset+(y+4)*scale}h{scale}v{scale}h-{scale}Z"/>')
   path=out/f'source-{index}.svg';path.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200" viewBox="0 0 200 200">'+''.join(parts)+'</svg>');generated.append(path);qr_sources[url]=str(path.relative_to(ROOT))
 ui=(ROOT/'firmware/OrbitS3/orbit_ui.cpp').read_text()
 # Absolute geometry is extracted from the real LVGL construction calls.
 rects={}
 for identifier,symbol in {'introSubject':'intro_subject','introContext':'intro_context','introReadSubject':'intro_read_subject','introViewport':'intro_viewport','introSourceTitle':'intro_source_title','introSourceDomain':'intro_source_domain','volumeNumber':'number','weatherCity':'weather_city','weatherTemperature':'weather_temperature','weatherCondition':'weather_condition','weatherRange':'weather_range','weatherDetails':'weather_details','weatherWind':'weather_wind','weatherHumidity':'weather_humidity','weatherPrecip':'weather_precip','weatherAge':'weather_age','launchAppleMusic':'iphone_button','launchStatus':'iphone_status','volumeChip':'chip'}.items():
  match=re.search(r'\b'+symbol+r'=(label|metadata_label|box|button)\([^;\n]*',ui)
  assert match,'Missing native geometry: '+symbol
  args=match[0].split(',',2 if match[1] in ('label','button') else 1)[-1]
  values=re.match(r'(\d+),(\d+),(\d+)(?:,(\d+))?',args)
  assert values,'Non-literal native geometry needs review: '+symbol
  rects[identifier]=[int(v) for v in values.groups() if v is not None]
 frame_ms=int(re.search(r'now-frame_at\)>=(\d+)',ui)[1]);frame_count=int(re.search(r'FrameCount=(\d+)',(ROOT/'firmware/OrbitS3/status_identity.h').read_text())[1])
 contract={'firmwareVersion':version(),'mode':'offline_simulation','rects':rects,'statusFrameMs':frame_ms,'statusFrames':frame_count,'glyphs':glyphs,'sourceQr':qr_sources,
  'inputs':{name:digest(ROOT/name) for name in INPUTS},'assets':{str(path.relative_to(ROOT)):digest(path) for path in generated},
  'intentionalDifferences':['Synthetic local data; no API/device or geolocation requests','Browser brightness approximates hardware backlight','Sources use native-encoded fixture QR codes; other synthetic URLs use a clickable URL fallback','Browser text anti-aliasing/physical color differs; pinned Montserrat and native icon masks are shared']}
 css=(out/'layout.css').read_text();runtime=(out/'runtime.js').read_text()
 for name in PAGES:
  path=ROOT/name;text=path.read_text()
  lyric_data=json.loads((ROOT/'assets/lyrics/preview-data.json').read_text())
  lyric_block='// orbit-lyrics-preview:start\nconst lyricsPreviewCatalog='+script_json(lyric_data)+';\n'+(ROOT/'assets/lyrics/preview-model.js').read_text()+'\n'+(ROOT/'assets/lyrics/preview-ui.js').read_text()+'\n// orbit-lyrics-preview:end'
  text=re.sub(r'// orbit-lyrics-preview:start[\s\S]*?// orbit-lyrics-preview:end',lambda _:lyric_block,text,count=1)
  text=re.sub(r'/\* orbit-preview-parity:start \*/[\s\S]*?/\* orbit-preview-parity:end \*/','',text)
  scoped=scoped_css(css,name)
  text=text.replace('</style>','/* orbit-preview-parity:start */\n'+scoped+'\n/* orbit-preview-parity:end */\n</style>',1)
  block='// orbit-preview-parity:start\nconst previewContract='+json.dumps(contract,separators=(',',':'))+';\n'+runtime+'\n// orbit-preview-parity:end'
  if '// orbit-preview-parity:start' in text:text=re.sub(r'// orbit-preview-parity:start[\s\S]*?// orbit-preview-parity:end',lambda _:block,text)
  else:
   anchor='\n</script>' if name=='index.html' else '\n})();\n</script>'
   assert anchor in text;text=text.replace(anchor,'\n'+block+anchor,1)
  text=re.sub(r'(<dt>Firmware</dt>)<dd[^>]*>[^<]+</dd>',r'\1<dd id="aboutFirmware"></dd>',text)
  path.write_text(text)
 # One canonical screen and script: the alternate HTML keeps only its frame.
 canonical=(ROOT/PAGES[0]).read_text();alternate=(ROOT/PAGES[1]).read_text()
 a,b=screen_block(canonical);c,d=screen_block(alternate);alternate=alternate[:c]+canonical[a:b]+alternate[d:]
 definitions=re.search(r'<defs>([\s\S]*?)</defs>',canonical)[0]
 alternate=re.sub(r'<defs>[\s\S]*?</defs>',lambda _:definitions,alternate,count=1)
 alternate=re.sub(r'<script>[\s\S]*?</script>',lambda _:'<script>'+main_script(canonical)+'</script>',alternate,count=1)
 alternate=re.sub(r'<style>[\s\S]*?</style>',lambda _:'<style>'+alternate_style(canonical)+'</style>',alternate,count=1)
 for id in ('introMockTools','lyricsMockTools'):
  tool=re.search(r'<aside[^>]*id="'+id+r'"[^>]*>[\s\S]*?</aside>',canonical)[0]
  pattern=r'<aside[^>]*id="'+id+r'"[^>]*>[\s\S]*?</aside>'
  if re.search(pattern,alternate):alternate=re.sub(pattern,lambda _:tool,alternate,count=1)
  else:alternate=alternate.replace('<script src="assets/clock/pixel-glyphs.js">',tool+'\n<script src="assets/clock/pixel-glyphs.js">',1)
 if '<title>' not in alternate:alternate=alternate.replace('<meta charset="utf-8">','<meta charset="utf-8">\n<title>S3 / Orbit — Round controller</title>',1)
 navigation=re.search(r'<nav class="modes"[\s\S]*?</nav>',canonical)[0]
 alternate=re.sub(r'<nav class="modes"[\s\S]*?</nav>',lambda _:navigation,alternate,count=1)
 alternate=alternate.replace('480 × 480 · Simulated controls · S3 disconnected','Local simulation · No device connected')
 (ROOT/PAGES[1]).write_text(alternate)
 (out/'manifest.json').write_text(json.dumps(contract,indent=2)+'\n')
 return check()

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
 value=check() if args.check else generate();print('PASS: both offline previews synchronized with firmware '+value['firmwareVersion'])
