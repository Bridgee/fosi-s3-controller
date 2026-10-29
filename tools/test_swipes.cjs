const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),cases=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));let first;
for(const name of ['index.html','orbit-s3-preview.html']){
 const s=fs.readFileSync(path.join(root,name),'utf8').match(/\/\/ orbit-swipe-detector:start([\s\S]*?)\/\/ orbit-swipe-detector:end/)[1];
 if(first)assert.equal(s,first);else first=s;
 const {SwipeDetector,swipeDestination}=new Function(s+';return {SwipeDetector,swipeDestination}')();
 const ids={none:0,left:1,right:2,up:3,down:4};
 for(const c of cases){const g=new SwipeDetector();g.begin(c.x1,c.y1,c.start);assert.equal(ids[g.end(c.x2,c.y2,(c.start+c.elapsed)>>>0)],c.expected);assert.equal(g.active,false);assert.equal(g.end(c.x2,c.y2,(c.start+c.elapsed)>>>0),'none');}
 const returns={'blank-right':'right','blank-left':'left','blank-down':'down','blank-up':'up'};
 for(const [direction,page] of Object.entries({left:'blank-right',right:'blank-left',up:'blank-down',down:'blank-up'})){
  assert.equal(swipeDestination('player',direction),page);
  for(const d of ['left','right','up','down','none'])assert.equal(swipeDestination(page,d),d===returns[page]?'player':page);
 }
 const g=new SwipeDetector();g.begin(240,240,0);g.move(140,240);g.move(240,240);assert(g.moved);assert.equal(g.end(240,240,300),'none');
 g.begin(240,240,0);g.cancel();assert.equal(g.end(100,240,300),'none');
}
console.log('PASS: both preview swipe classifiers match native cases; mirrored source and four-page return topology');

// Exercise the actual preview input controller, including controls overlapping
// the arc, release-only drags, endpoints on the arc, and satellite-page returns.
const vm=require('node:vm');let controllerSource;
for(const name of ['index.html','orbit-s3-preview.html']){
 const html=fs.readFileSync(path.join(root,name),'utf8');
 const input=html.match(/\/\/ orbit-swipe-input:start([\s\S]*?)\/\/ orbit-swipe-input:end/)[1];
 if(controllerSource)assert.equal(input,controllerSource);else controllerSource=input;
 const reading=html.match(/\/\/ orbit-music-intro:start([\s\S]*?)\/\/ orbit-music-intro:end/)[1];
 const detector=html.match(/\/\/ orbit-swipe-detector:start([\s\S]*?)\/\/ orbit-swipe-detector:end/)[1];
 function fixture(view='player'){
  const handlers={},state={view,standby:false,volume:38,track:0},nodes={};let time=100,writes=0;
  const empty={left:0,top:0,right:0,bottom:0,width:0,height:0};
  const screen={getBoundingClientRect:()=>({left:0,top:0,width:480,height:480}),addEventListener:(n,f)=>handlers[n]=f,setPointerCapture:()=>{},classList:{contains:()=>false}};
  const context={tracks:[{title:'Track',artist:'Artist',album:'Album'}],canPlay:()=>true,requestAnimationFrame:()=>1,cancelAnimationFrame:()=>{},state,artworkViewing:false,dragging:false,lastAngle:0,gestureVolume:38,performance:{now:()=>time},
   $:id=>id==='screen'?screen:(nodes[id]||(nodes[id]={scrollTop:0,scrollHeight:700,clientHeight:236,getBoundingClientRect:()=>id==='muteSmall'?{left:410,top:210,right:450,bottom:270,width:40,height:60}:id==='musicExplore'?{left:65,top:276,right:415,bottom:336,width:350,height:60}:empty})),
   getComputedStyle:()=>({visibility:'visible'}),document:{querySelectorAll:()=>[],elementFromPoint:()=>({closest:()=>true})},
   show:v=>state.view=v,slideTo:v=>state.view=v,setVolume:v=>{state.volume=Math.round(v);writes++}};
  vm.runInNewContext(detector+'\n'+reading+'\n'+input+";introMock.state='ready';",context);
  function event(type,x,y,pointerType='touch',delay=75){time+=delay;let stopped=false;handlers[type]({isPrimary:true,button:0,pointerId:1,pointerType,clientX:x,clientY:y,preventDefault:()=>stopped=true,stopImmediatePropagation:()=>stopped=true});return stopped}
  return {state,event,writes:()=>writes,scroll:()=>nodes.introViewport?.scrollTop||0};
 }
 // Browser dragstart must be prevented before native drag can pointercancel.
 const nativeDrag=fixture();assert.equal(nativeDrag.event('dragstart',240,160,'mouse'),true);
 // All four return paths: fast/slow desktop drags, off-display release, and
 // physical-touch timing still differ deliberately at the adapter boundary.
 for(const [dx,dy,page] of [[110,0,'blank-left'],[-110,0,'blank-right'],[0,110,'blank-up'],[0,-110,'blank-down']]){
  for(const duration of [75,600,1600,3500])for(const outside of [false,true]){
   const f=fixture();f.event('pointerdown',240,240,'mouse');f.event('pointermove',240+dx,240+dy,'mouse',duration);f.event('pointerup',240+dx,240+dy,'mouse');assert.equal(f.state.view,page);
   f.event('pointerdown',240,240,'mouse');
   const scale=outside?3:1;f.event('pointermove',240-dx*scale,240-dy*scale,'mouse',duration);f.event('pointerup',240-dx*scale,240-dy*scale,'mouse');assert.equal(f.state.view,'player');assert.equal(f.writes(),0);
  }
  const touch=fixture(page);touch.event('pointerdown',240,240);touch.event('pointermove',240-dx,240-dy,'touch',1600);touch.event('pointerup',240-dx,240-dy);assert.equal(touch.state.view,page);
  const canceled=fixture(page);canceled.event('pointerdown',240,240,'mouse');canceled.event('pointercancel',240,240,'mouse');canceled.event('pointerup',240-dx,240-dy,'mouse');assert.equal(canceled.state.view,page);
 }
 // Execute the actual reading controller together with the actual drag helpers.
 const read=fixture('intro-read');read.event('pointerdown',240,330);read.event('pointermove',240,260);assert.equal(read.scroll(),70);assert.equal(read.state.view,'intro-read');read.event('pointerup',240,240);assert.equal(read.writes(),0);
 const back=fixture('intro-read');back.event('pointerdown',240,230);back.event('pointermove',350,230);back.event('pointerup',350,230);assert.equal(back.state.view,'intro');assert.equal(back.writes(),0);
 // A metadata corner overlaps the geometric arc: its tap is not a volume tap,
 // while a deliberate arc-origin drag retains the established volume priority.
 for(const [x,y] of [[65,326],[414,326]]){
  const tap=fixture();tap.event('pointerdown',x,y);tap.event('pointerup',x,y);assert.equal(tap.state.view,'player');assert.equal(tap.writes(),0);
  const drag=fixture();drag.event('pointerdown',x,y);drag.event('pointermove',240,240);drag.event('pointerup',240,240);assert.equal(drag.state.view,'volume');
 }
 for(const [x,y] of [[435,240],[45,240],[240,48],[83,397],[397,397]]){
  for(const releaseOnly of [false,true]){
   const f=fixture();f.event('pointerdown',x,y);assert.equal(f.state.view,'player');
   if(!releaseOnly)f.event('pointermove',(x+240)/2,(y+240)/2);
   f.event('pointerup',240,240);assert.equal(f.state.view,'volume');
  }
 }
 const f=fixture();f.event('pointerdown',435,240);f.event('pointermove',435,190);f.event('pointerup',435,190);
 assert.equal(f.state.view,'volume');assert(f.writes()>0);assert(f.state.volume<38);
 const inner=fixture();inner.event('pointerdown',150,240);inner.event('pointermove',300,240);inner.event('pointerup',435,240);
 assert.equal(inner.state.view,'blank-left');assert.equal(inner.writes(),0);
 const satellite=fixture('blank-right');satellite.event('pointerdown',17,240);satellite.event('pointermove',90,240);satellite.event('pointerup',130,240);assert.equal(satellite.state.view,'player');assert.equal(satellite.writes(),0);
 for(const [dx,dy,page]of [[110,0,'blank-left'],[-110,0,'blank-right'],[0,110,'blank-up'],[0,-110,'blank-down']]){
  const sleeping=fixture();sleeping.state.standby=true;
  sleeping.event('pointerdown',240,240);sleeping.event('pointermove',240+dx,240+dy);sleeping.event('pointerup',240+dx,240+dy);
  assert.equal(sleeping.state.view,page);assert.equal(sleeping.writes(),0);
  sleeping.event('pointerdown',240,240);sleeping.event('pointermove',240-dx,240-dy);sleeping.event('pointerup',240-dx,240-dy);
  assert.equal(sleeping.state.view,'player');assert.equal(sleeping.writes(),0);
 }
 const sleepingRing=fixture();sleepingRing.state.standby=true;sleepingRing.event('pointerdown',240,48);sleepingRing.event('pointermove',240,148);sleepingRing.event('pointerup',240,158);assert.equal(sleepingRing.state.view,'blank-up');assert.equal(sleepingRing.writes(),0);
 const tap=fixture();tap.event('pointerdown',435,240);assert.equal(tap.event('pointerup',435,240),false);assert.equal(tap.state.view,'player');assert.equal(tap.writes(),0);
}
console.log('PASS: actual preview controllers: arc drag priority over overlapping controls, release-only/relative volume, inner-to-arc navigation, satellite return and unchanged button taps');

console.log('PASS: metadata entry is included in preview arc-tap exclusion; drag ownership unchanged');

console.log("PASS: native browser drag blocked; four desktop round trips at75–3500ms with inside/outside release, pointer cancellation and unchanged touch timeout; both entry points");
