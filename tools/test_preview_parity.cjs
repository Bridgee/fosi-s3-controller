// Offline VM tests of the exact shared adapter installed in both HTML previews.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..'),manifest=JSON.parse(fs.readFileSync(path.join(root,'assets/preview/manifest.json')));
let count=0,first;
function eq(actual,expected){assert.deepEqual(actual,expected);count++}
function environment(search=''){
 const nodes={},timers=[],classes=()=>{const set=new Set();return {add:k=>set.add(k),remove:k=>set.delete(k),contains:k=>set.has(k),toggle:(k,v)=>{v=v??!set.has(k);if(v)set.add(k);else set.delete(k);return v}}};
 function node(id=''){return {id,textContent:'',dataset:{},style:{},attrs:{},classList:classes(),children:[],disabled:false,hidden:false,setAttribute(k,v){this.attrs[k]=String(v)},getAttribute(k){return this.attrs[k]??null},append(n){this.children.push(n)},replaceChildren(...children){this.children=children},querySelector(s){return s==='img'?this.children.find(c=>c.tag==='img')||null:null},querySelectorAll(s){return s==='dd'?about:s==='button'?transport:[]},closest:()=>stage}}
 const about=Array.from({length:8},()=>node()),transport=Array.from({length:3},()=>node()),stage=node(),glyph=node();
 const $=id=>nodes[id]||(nodes[id]=node(id));$('screen').querySelectorAll=s=>s.startsWith('[data-input]')?routes:[glyph];
 const routes=[node(),node()];let now=0,volumeWrites=0,launches=0,trackChanges=0,notices=0;
 const state={view:'player',input:'network',standby:false,playing:true,muted:false,volume:38,elapsed:4,artworkZoomed:false};
 const context={$,state,location:{search},URLSearchParams,document:{hidden:false,createElement:tag=>Object.assign(node(),{tag})},performance:{now:()=>now},setInterval:(f,ms)=>{timers.push({f,ms});return timers.length},pageTransitionActive:false,
 canPlay:()=>!state.standby&&state.input==='network',setVolume:v=>{volumeWrites++;state.volume=v},toggleMute(){},togglePlay(){},changeTrack:()=>trackChanges++,notice:()=>notices++,currentSource:()=>({label:'AirPlay'}),render(){},renderClockPreview(){},renderLaunch(){},simulateLaunch:()=>launches++,pixelText:(id,text)=>$(id).textContent=text,
 footerStatus:(mode,label)=>{$('deviceStatus').dataset.state=mode;$('deviceStatus').setAttribute('aria-label',label)},steadyFooterStatus(){},
 introMock:{state:'ready',value:{sources:[{title:'Sample',url:'https://example.org/local-introduction'}]}},introSourceIndex:0,
 renderIntroSource(){const ref=context.introMock.value?.sources[context.introSourceIndex],url=ref?.url||'';if($('introSourceLink').dataset.url!==url){$('introSourceLink').replaceChildren();$('introSourceLink').dataset.url=url;$('introSourceLink').textContent=url}}};
 vm.createContext(context);const adapter=fs.readFileSync(path.join(root,'assets/preview/runtime.js'),'utf8');vm.runInContext('const previewContract='+JSON.stringify(manifest)+';'+adapter,context);
 const run=s=>vm.runInContext(s,context);
 return {$,state,about,transport,routes,stage,glyph,context,run,timers,advance:ms=>{now+=ms;timers.forEach(t=>t.f())},stats:()=>({volumeWrites,launches,trackChanges,notices})};
}
for(const page of ['index.html','orbit-s3-preview.html']){
 const text=fs.readFileSync(path.join(root,page),'utf8');for(const m of text.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g))new Function(m[1]);
 const block=text.match(/\/\/ orbit-preview-parity:start([\s\S]*?)\/\/ orbit-preview-parity:end/)[1];if(first)eq(block,first);first=block;
 assert(!/fetch\(|XMLHttpRequest|api\.openai|api\.day\.app|\.serial|requestDevice/.test(block));count++;
 const ids=new Set([...text.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]));for(const m of text.matchAll(/\$\('([^']+)'\)/g))assert(ids.has(m[1]),page+' missing '+m[1]);count++;
 assert(text.indexOf('const introMock=createIntroMock()')<text.indexOf('configureIntroMocks(previewParams)'));count++;
 assert(text.includes("!['offline','locating','syncing'].includes(previewParams.get('clock'))&&slot.enabled"));count++;
 assert(!/0\.1\.18|Sample weather|Reserved. Not active on device./.test(text));count++;
}
const e=environment();eq(e.$('screen').dataset.firmware,manifest.firmwareVersion);eq(e.about[2].textContent,'Connected');eq(e.about[7].textContent,manifest.firmwareVersion);
for(const [id,rect] of Object.entries(manifest.rects)){eq(e.$(id).style.left,rect[0]+'px');eq(e.$(id).style.top,rect[1]+'px');eq(e.$(id).style.width,rect[2]+'px')}
eq(e.$('deviceStatus').dataset.state,'connected');eq(e.transport.every(b=>!b.disabled),true);e.$('muteSmall').onclick();eq(e.state.muted,true);e.run('toggleMute()');eq(e.state.muted,false);e.run('setVolume(39)');eq(e.state.volume,39);
e.state.input='bluetooth';e.run('render()');eq(e.run('canPlay()'),false);eq(e.transport.every(b=>b.disabled),true);eq(e.$('vinylDisc').style.animationPlayState,'running');eq(e.$('aboutInputKey').textContent,'Input');eq(e.$('aboutInputValue').textContent,'Bluetooth');eq(e.$('aboutStatus').textContent,'');e.run('togglePlay()');eq(e.state.playing,true);e.run('changeTrack(1)');eq(e.stats().trackChanges,0);
e.state.playing=false;e.run('render()');eq(e.$('vinylDisc').style.animationPlayState,'paused');e.state.bluetoothConnected=false;e.run('render()');eq(e.about[5].textContent,'Not connected');eq(e.$('deviceStatus').dataset.state,'connected');
const bt=environment('?btControl=1');bt.state.input='bluetooth';bt.run('render()');bt.$('play').onclick();eq(bt.state.playing,false);eq(bt.transport.every(b=>!b.disabled),true);
for(const [input,label] of [['line','Line In signal'],['hdmi','HDMI signal'],['optical','Optical signal']]){e.state.input=input;e.run('render()');eq(e.$('aboutInputKey').textContent,label);eq(e.$('aboutInputValue').textContent,'Unknown');eq(e.$('aboutStatus').textContent,'');eq(e.transport.every(b=>b.disabled),true)}
e.state.standby=true;e.run('steadyFooterStatus();render()');eq(e.$('deviceStatus').dataset.state,'sleep');eq(e.about[3].textContent,'Standby');const old=e.state.volume;e.run('setVolume(45)');eq(e.state.volume,old);
for(const query of ['?link=offline','?wifi=offline']){const x=environment(query);eq(x.transport.every(b=>b.disabled),true);eq(x.$('deviceStatus').dataset.state,'offline');eq(x.about[1].textContent,'Not found');eq(x.$('volumeNumber').textContent,'--');x.run('setVolume(90);toggleMute();simulateLaunch()');eq(x.stats().volumeWrites,0);eq(x.stats().launches,0);eq(x.state.muted,false);eq(x.routes.every(b=>b.disabled),true);x.run('renderLaunch()');eq(x.$('launchAppleMusic').disabled,true)}
const ro=environment('?readOnly=1');eq(ro.$('minus').disabled,true);eq(ro.$('plus').disabled,true);eq(ro.$('volumeLimitHint').textContent,'Read only');ro.run('setVolume(60);toggleMute()');eq(ro.state.volume,38);eq(ro.state.muted,true);
const w=environment('?link=waiting');eq(w.$('deviceStatus').dataset.state,'waiting');const mark=w.$('deviceStatus').children[0];eq(mark.style.maskImage,'url("assets/preview/status-waiting_0.svg")');w.advance(150);eq(mark.style.maskImage,'url("assets/preview/status-waiting_1.svg")');w.advance(1650);eq(mark.style.maskImage,'url("assets/preview/status-waiting_0.svg")');
for(const [mode,label] of [['pairing','Pairing mode'],['searching','Searching'],['unknown','Unknown'],['disconnected','Not connected']]){const x=environment('?bluetooth='+mode);x.state.input='bluetooth';x.run('render()');eq(x.about[5].textContent,label);eq(x.$('vinylDisc').style.animationPlayState,undefined)}
for(const mode of ['error','limited']){const x=environment('?link='+mode);eq(x.$('deviceStatus').dataset.state,mode);assert(x.$('aboutStatus').textContent);count++}
for(const mode of ['offline','locating','syncing']){const x=environment('?clock='+mode);x.state.view='blank-up';x.run('renderClockPreview()');eq(x.$('pixelTime').textContent,'--:--');eq(x.run('clockPreviewReady()'),false);eq(x.$('pixelStatus').textContent,{offline:'CONNECT TO WI-FI',locating:'FINDING LOCATION',syncing:'SYNCING TIME'}[mode])}
for(const url of Object.keys(manifest.sourceQr)){e.context.introMock.value.sources=[{title:'Sample',url}];e.run('renderIntroSource()');const image=e.$('introSourceLink').children[0];eq(image.getAttribute('src'),manifest.sourceQr[url]);e.run('renderIntroSource()');eq(e.$('introSourceLink').children[0],image)}
e.context.introMock.value.sources=[{title:'Other local fixture',url:'https://example.net/sample'}];e.run('renderIntroSource()');eq(e.$('introSourceLink').classList.contains('qr-source'),false);eq(e.$('introSourceLink').textContent,'https://example.net/sample');
console.log('PASS: '+count+' preview parity assertions: shared adapter, native geometry/version, connection, controls, BT, physical sources, standby, readonly, status frames, clock readiness and retained source QR. No network/device/API calls.');
