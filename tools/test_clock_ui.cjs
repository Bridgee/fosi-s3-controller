// Deterministic tests of the real browser idle adapter; no wall-clock waits/APIs.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),idle=fs.readFileSync(path.join(root,'assets/screen-idle/preview.js'),'utf8');
let checks=0;const eq=(a,b)=>{assert.deepEqual(a,b);checks++};
function fixture(view='blank-up'){
 let now=0,next=0;const timers=new Map(),events=new Map(),world=new Map(),classes=new Set();
 const screen={classList:{add:n=>classes.add(n),remove:n=>classes.delete(n),contains:n=>classes.has(n)},addEventListener:(t,f)=>events.set(t,[...(events.get(t)||[]),f])};
 const state={view};const context={state,window:{addEventListener:(t,f)=>world.set(t,[...(world.get(t)||[]),f])},setTimeout:(f,delay)=>{const id=++next;timers.set(id,{f,at:now+delay});return id},clearTimeout:id=>timers.delete(id),settlePageTransition(){},renderMetadataMotion(){},renderClockPreview(){},renderWeatherPreview(){},screen,root:{contains:()=>true}};
 vm.runInNewContext(idle+';installScreenIdle(screen,30000,root);',context);
 const send=(map,type)=>{const event={target:{},stopped:false,preventDefault(){this.stopped=true},stopImmediatePropagation(){this.stopped=true}};for(const f of map.get(type)||[])f(event);return event.stopped};
 const advance=delta=>{const end=now+delta;for(;;){const due=[...timers].filter(([,t])=>t.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];if(!due)break;timers.delete(due[0]);now=due[1].at;due[1].f()}now=end};
 return {state,advance,has:n=>classes.has(n),send:t=>send(events,t),world:t=>send(world,t),show:v=>{state.view=v;send(events,'orbit-view-change')}};
}
const c=fixture();c.advance(29999);eq(c.has('display-dimmed'),false);c.advance(1);eq(c.has('display-dimmed'),true);eq(c.has('display-asleep'),false);c.advance(120000);eq(c.has('display-dimmed'),true);
eq(c.send('pointerdown'),true);eq(c.has('display-dimmed'),false);eq(c.send('pointermove'),true);eq(c.world('pointerup'),true);eq(c.send('click'),true);eq(c.send('pointerdown'),false);eq(c.world('pointerup'),false);eq(c.send('click'),false);
c.advance(30000);eq(c.has('display-dimmed'),true);c.send('orbit-clock-alert');eq(c.has('display-dimmed'),false);eq(c.has('display-asleep'),false);
c.advance(30000);c.show('clock-alert');c.advance(120000);eq(c.has('display-asleep'),false);eq(c.has('display-dimmed'),false);
c.show('blank-up');c.advance(30000);eq(c.has('display-dimmed'),true);c.show('player');eq(c.has('display-dimmed'),false);c.advance(30000);eq(c.has('display-asleep'),true);
for(const view of ['clock-alarm','clock-timer','player','routing','theme','about']){const f=fixture(view);f.advance(30000);eq(f.has('display-asleep'),true);eq(f.has('display-dimmed'),false);}
const held=fixture();held.send('pointerdown');held.advance(120000);eq(held.has('display-dimmed'),false);held.world('pointerup');held.advance(30000);eq(held.has('display-dimmed'),true);
for(const page of ['index.html','orbit-s3-preview.html']){
 const html=fs.readFileSync(path.join(root,page),'utf8');assert(html.includes(idle));checks++;
 assert(/<button class="pixel-control" id="pixelAlarmValue"/.test(html));checks++;
 assert(!/SET ALARM|NEXT ALARM|ALARM OFF/.test(html));checks++;
 assert(html.includes("clockSlots=Array.from({length:4}")&&html.includes("clockSlotName(i)"));checks++;
}
console.log(`PASS: ${checks} clock UI cases: four alarm/timer selectors, saved-time edit entry, outer run switch, dim-only clock, wake-only touch, alerts, held contact and normal sleep elsewhere; no API/device access.`);
