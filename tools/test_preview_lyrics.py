"""Cross-check preview annotation matching with the actual bounded firmware parser.
Synthetic title pairs only; no providers, devices, private headers or network.
"""
from pathlib import Path
import argparse,json,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--node',default='node');args=p.parse_args()
pairs=[]
notes=['(Remastered 2009)','[remaster]','{Digitally Remastered 2009}','（重制版）','【重製版】','「リマスター」','〈2009〉','《notes》','(Live)','(liveat Tokyo)','(Remix)','(Extended mix)','(Acoustic)','(Japanese Version)','(日文版)','(ライブ)','(featuring Another Artist)','(Part 2)','- 2009 Remaster','— Remastered','– Live','Remastered 2009','(2009 Remaster Live)','(Different note)','(Unclosed','(Original version)']
for title in ['A Place Between','Canción','Cafe\u0301','東京の歌']:
 for note in notes:pairs.append((title+' '+note,title))
pairs.extend([('Song (note) [other]','Song'),('Song-Live','Song'),('Song-remastered','Song'),('Test\u00a0','Test'),('TEST','test'),('  Cafe\u0301  ','Café'),('Song (Live)','Song (Live)')])
with tempfile.TemporaryDirectory(prefix='preview-lyric-parity-') as tmp:
 tmp=Path(tmp);cases=','.join('{'+json.dumps(a,ensure_ascii=False)+','+json.dumps(b,ensure_ascii=False)+'}' for a,b in pairs)
 (tmp/'main.cpp').write_text('#include "lyrics_parser.h"\n#include <stdio.h>\nstruct Pair{const char *a,*b;};int main(){Pair pairs[]={'+cases+'};for(const auto &p:pairs)printf("%d\\n",int(orbit_lyrics::same_title(p.a,p.b)));}\n')
 binary=tmp/'compare';subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+str(r/'firmware/OrbitS3'),str(tmp/'main.cpp'),str(r/'firmware/OrbitS3/lyrics_parser.cpp'),str(r/'firmware/OrbitS3/metadata_text.cpp'),'-o',str(binary)],check=True)
 native=subprocess.check_output([str(binary)],text=True).splitlines();assert len(native)==len(pairs)
 values=[dict(a=a,b=b,expected=result=='1') for (a,b),result in zip(pairs,native)]
 (tmp/'cases.json').write_text(json.dumps(values,ensure_ascii=False))
 subprocess.run([args.node,str(r/'tools/test_preview_lyrics.cjs'),str(tmp/'cases.json')],check=True)
print('PASS: '+str(len(pairs))+' native/browser annotation and version-retention pairs; actual firmware parser under ASan/UBSan, synthetic data only.')
