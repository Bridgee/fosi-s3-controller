// Run real transition timers + input guards + swipe controller together.
// No browser automation, networking, private configuration or device access.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');let tests=0;
for(const page of ['index.html','orbit-s3-preview.html']){
 const html=fs.readFileSync(path.join(root,page),'utf8');
 const block=name=>html.match(new RegExp('// '+name+':start([\\s\\S]*?)// '+name+':end'))[1];
 function fixture(){
  const handlers={},windowHandlers={},timers=new Map(),classes=new Set(),state={view:'player',standby:false,volume:38};let time=100,nextTimer=1,audioWrites=0;
  const register=map=>(type,f)=>{(map[type]??=[]).push(f)};
  const screen={addEventListener:register(handlers),closest:()=>null,contains:()=>true,setPointerCapture(){},getBoundingClientRect:()=>({left:0,top:0,width:480,height:480}),classList:{add:(...names)=>names.forEach(n=>classes.add(n)),remove:(...names)=>names.forEach(n=>classes.delete(n)),contains:n=>classes.has(n)},style:{setProperty(){},removeProperty(){}}};
  let context;const renderShow=view=>{vm.runInContext('stopPageTransition()',context);state.view=view};
  context={state,$:id=>id==='screen'?screen:{getBoundingClientRect:()=>({left:0,top:0,right:0,bottom:0,width:0,height:0})},performance:{now:()=>time},matchMedia:()=>({matches:false}),getComputedStyle:()=>({visibility:'visible'}),document:{contains:()=>true,elementFromPoint:()=>null},window:{addEventListener:register(windowHandlers)},setTimeout:(f,ms)=>{const id=nextTimer++;timers.set(id,{at:time+ms,f});return id},clearTimeout:id=>timers.delete(id),show:renderShow,artworkViewing:false,dragging:false,lastAngle:0,gestureVolume:38,setVolume:()=>audioWrites++,stopIntroMotion(){},introReadContact:()=>false,beginIntroDrag(){},moveIntroDrag(){},endIntroDrag(){}};
  vm.createContext(context);vm.runInContext(block('orbit-swipe-detector')+'\n'+block('orbit-page-transition')+'\n'+block('orbit-page-transition-input')+'\n'+block('orbit-swipe-input'),context);
  function dispatch(type,x=240,y=240,delay=0){time+=delay;let stopped=false,prevented=false;const event={isPrimary:true,button:0,pointerId:1,pointerType:'mouse',clientX:x,clientY:y,target:screen,preventDefault(){prevented=true},stopImmediatePropagation(){stopped=true}};for(const map of [windowHandlers,handlers])for(const f of map[type]||[]){if(stopped)break;f(event)}return {stopped,prevented}}
  function advance(ms){const until=time+ms;while(true){let first;for(const [id,t] of timers)if(t.at<=until&&(!first||t.at<first[1].at))first=[id,t];if(!first)break;timers.delete(first[0]);time=first[1].at;first[1].f()}time=until}
  function swipe(dx,dy){dispatch('pointerdown');dispatch('pointermove',240+dx,240+dy,75);dispatch('pointerup',240+dx,240+dy,75)}
  return {state,dispatch,swipe,advance,active:()=>vm.runInContext('pageTransitionActive',context),writes:()=>audioWrites,timers};
 }
 for(const [dx,dy,destination] of [[110,0,'blank-left'],[-110,0,'blank-right'],[0,110,'blank-up'],[0,-110,'blank-down']]){
  for(const elapsed of [0,110])for(let repeat=0;repeat<20;repeat++){
   const f=fixture();f.swipe(dx,dy);assert(f.active());f.advance(elapsed);
   f.swipe(-dx,-dy);f.advance(400);assert.equal(f.state.view,'player');assert.equal(f.active(),false);assert.equal(f.timers.size,0);assert.equal(f.writes(),0);tests++;
  }
  // A tap that interrupts animation settles only, never activates a target.
  const tap=fixture();tap.swipe(dx,dy);tap.dispatch('pointerdown');assert.equal(tap.state.view,destination);assert(tap.dispatch('click').stopped);tap.dispatch('pointerup');tap.dispatch('pointerdown');tap.dispatch('pointerup');assert.equal(tap.dispatch('click').stopped,false);assert.equal(tap.writes(),0);tests++;
  const canceled=fixture();canceled.swipe(dx,dy);canceled.dispatch('pointerdown');canceled.dispatch('pointercancel');canceled.advance(400);canceled.swipe(-dx,-dy);canceled.advance(400);assert.equal(canceled.state.view,'player');assert.equal(canceled.active(),false);tests++;
 }
}
console.log('PASS: '+tests+' integrated transition/input cases: immediate reversal during exit/entry, settled pointer ownership, tap suppression, cancellation, fresh taps, timer cleanup and no audio writes; both previews.');
