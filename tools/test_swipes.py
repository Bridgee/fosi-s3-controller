"""Native and preview classifier parity on edge cases, time wrap and malformed coordinates."""
from pathlib import Path
import argparse,json,random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--node',default='node');args=p.parse_args()
cases=[]
def add(x1,y1,x2,y2,elapsed=300,start=100):
 expected=0
 if all(0<=v<480 for v in [x1,y1,x2,y2]) and 30<=elapsed<=1200:
  dx,dy=x2-x1,y2-y1;ax,ay=abs(dx),abs(dy)
  if ax>=48 and ax*2>=ay*3:expected=1 if dx<0 else 2
  elif ay>=48 and ay*2>=ax*3:expected=3 if dy<0 else 4
 cases.append(dict(x1=x1,y1=y1,x2=x2,y2=y2,start=start,elapsed=elapsed,expected=expected))
for dx,dy in [(-100,0),(100,0),(0,-100),(0,100),(-48,32),(-48,33),(32,48),(33,48),(-47,0),(47,0),(80,80),(0,0),(14,0),(15,15)]:
 for duration in [0,29,30,300,1200,1201]:add(240,240,240+dx,240+dy,duration)
for start in [0xfffffff0,0xffffff00]:
 for dx,dy in [(-100,0),(100,0),(0,-100),(0,100)]:add(240,240,240+dx,240+dy,300,start)
for p1,p2 in [((-1,0),(100,100)),((0,0),(480,100)),((240,240),(240,480)),((-32768,-32768),(479,479))]:add(*p1,*p2)
rng=random.Random(325118)
for _ in range(600):add(*(rng.randrange(-50,530) for i in range(4)),rng.randrange(0,1600))
with tempfile.TemporaryDirectory(prefix='orbit-swipes-') as d:
 w=Path(d);(w/'cases.json').write_text(json.dumps(cases));(w/'swipe_cases.h').write_text('struct Case {int x1,y1,x2,y2;uint32_t start,elapsed;int expected;};\nstatic const Case cases[]={\n'+''.join('{%d,%d,%d,%d,%du,%du,%d},\n'%(c['x1'],c['y1'],c['x2'],c['y2'],c['start'],c['elapsed'],c['expected']) for c in cases)+'};\n')
 binary=w/'swipe-test';subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(w),'-I'+str(r/'firmware/OrbitS3'),str(r/'tests/swipe_gesture_test.cpp'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
 subprocess.run([args.node,str(r/'tools/test_swipes.cjs'),str(w/'cases.json')],check=True)
