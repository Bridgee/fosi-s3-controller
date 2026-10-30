// Local-only integration tests for both standalone previews; time is injected.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');let first;
for(const file of ['index.html','orbit-s3-preview.html']){
 const source=fs.readFileSync(path.join(root,file),'utf8');
 for(const script of source.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g))new Function(script[1]);
 const crypto=require('node:crypto'),icons=JSON.parse(fs.readFileSync(path.join(root,'assets/music-icons/manifest.json'),'utf8'));
 assert.equal(icons.totalPixelBytes,3072);
 const compiledIcons=fs.readFileSync(path.join(root,'firmware/OrbitS3/intro_icons.cpp'),'utf8');
 for(const icon of icons.icons){
  const svg=fs.readFileSync(path.join(root,'assets/music-icons',icon.id+'.svg'),'utf8');assert.equal(crypto.createHash('sha256').update(svg).digest('hex'),icon.svgSha256);
  assert(source.includes(svg.slice(svg.indexOf('>')+1,svg.lastIndexOf('</svg>'))));
  const raw=compiledIcons.split('static const uint8_t '+icon.id+'_pixels[]={')[1].split('};')[0];const bytes=Buffer.from(raw.match(/\d+/g).map(Number));assert.equal(bytes.length,1024);assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),icon.alphaSha256);
 }
 const block=source.match(/\/\/ orbit-music-intro:start([\s\S]*?)\/\/ orbit-music-intro:end/)[1];
 if(first)assert.equal(block,first);else first=block;
 assert(!/fetch\(|XMLHttpRequest|s3\.send|simulateLaunch\(|localStorage|sessionStorage/.test(block));
 const nodes={},rows=[0,1,2].map(i=>({dataset:{introTopic:String(i)},disabled:false}));
 const $=id=>nodes[id]||(nodes[id]={textContent:'',dataset:{},hidden:false,disabled:false,scrollTop:0,scrollHeight:900,clientHeight:236,events:{},
  setAttribute(name,value){this[name]=value},classList:{contains:()=>false},querySelector:()=>null,addEventListener(type,f){(this.events[type]||=[]).push(f)}});
 const state={view:'player',track:0},tracks=[{title:'<img src=x onerror=alert(1)> / Canción',artist:'Un très long nom / 日本語',album:'Álbum',duration:240},{title:'Second track',artist:'New artist',album:'',duration:200}];
 let active=true,now=0,nextFrame=1,nextTimer=1;const frames=new Map(),timers=new Map();
 let ctx;const show=v=>{state.view=v;for(const f of $('screen').events['orbit-view-change']||[])f();vm.runInContext('renderIntro()',ctx)};
 ctx={$,state,tracks,URL,TextEncoder,performance:{now:()=>now},requestAnimationFrame:f=>{const id=nextFrame++;frames.set(id,f);return id},cancelAnimationFrame:id=>frames.delete(id),setInterval:f=>{let id=nextTimer++;timers.set(id,f);return id},clearInterval:id=>timers.delete(id),document:{documentElement:{classList:{toggle:()=>{}}},querySelectorAll:()=>rows},canPlay:()=>active,show};
 vm.createContext(ctx);vm.runInContext(block,ctx);
 const run=s=>vm.runInContext(s,ctx),advance=ms=>{now+=ms;for(const f of [...timers.values()])f()},reset=scenario=>{ctx.params={get:k=>k==='intro'?scenario:k==='introTools'?'1':null};run('configureIntroMocks(params)')};
 reset('disabled');$('musicExplore').onclick();for(const row of rows){row.onclick();assert.equal($('introMessage').textContent,'Introductions are off');assert($('introViewport').hidden&&$('introSourceList').hidden&&$('introRetry').hidden);assert.equal(run('introMock.loads'),0);$('introReadBack').onclick()}$('introBack').onclick();
 reset('success');$('musicExplore').onclick();assert.equal(state.view,'intro');assert.equal($('introSubject').textContent,tracks[0].title);assert(rows.every(r=>!r.disabled));
 rows[0].onclick();assert.equal(state.view,'intro-read');assert.equal($('introReadSubject').textContent,tracks[0].artist);assert.equal($('introKind').textContent,'Artist');
 assert.equal($('introMessage').textContent,'Loading');assert($('introViewport').hidden);advance(1499);assert.equal($('introMessage').textContent,'Loading');advance(1);
 assert(!$('introViewport').hidden&&$('introMessage').hidden);assert($('introBody').textContent && $('introParagraph2').textContent);assert.equal($('introFeedback').textContent,'Local sample');assert.equal(timers.size,0);
 run('beginIntroDrag(330,100);moveIntroDrag(285,175)');assert.equal($('introViewport').scrollTop,45);
 run('moveIntroDrag(240,250);endIntroDrag()');assert.equal($('introViewport').scrollTop,90);assert(frames.size);
 for(let t=16;t<800&&frames.size;t+=16){const [id,f]=frames.entries().next().value;frames.delete(id);f(t)}
 assert($('introViewport').scrollTop>=90&&$('introViewport').scrollTop<=664);
 run('beginIntroDrag(200,1000);moveIntroDrag(400,1075);endIntroDrag();stopIntroMotion()');assert.equal(frames.size,0);
 // All three topics survive a tour, Back to home and reopening, with zero loads.
 $('introReadBack').onclick();for(const topic of [1,2]){rows[topic].onclick();assert.equal($('introMessage').textContent,'Loading');advance(1500);assert(!$('introViewport').hidden);$('introReadBack').onclick()}
 const tourLoads=run('introMock.loads');assert.equal(tourLoads,3);assert.equal(run('introMock.cache.length'),3);
 for(const topic of [0,1,2]){rows[topic].onclick();assert.equal($('introFeedback').textContent,'Local sample | Saved');assert.equal(timers.size,0);$('introReadBack').onclick()}
 $('introBack').onclick();$('musicExplore').onclick();rows[0].onclick();assert.equal(run('introMock.loads'),tourLoads);assert.equal($('introFeedback').textContent,'Local sample | Saved');
 const beforeHeader=run('introData.album');run("introTopic=1;introData.album='';introMock.value.subject='Identified EP';renderIntro()");assert.equal($('introReadSubject').textContent,'Identified EP');run('renderIntro()');assert.equal($('introReadSubject').textContent,'Identified EP');ctx.restoreRelease=beforeHeader;run('introData.album=restoreRelease;introTopic=0;renderIntro()');
 // End-of-reading named source list reuses the ready value and scroll position.
 const sourceLoads=run('introMock.loads');$('introViewport').scrollTop=83;
 assert(!$('introSourceList').hidden && !$('introReference0').hidden && $('introReference1').hidden);assert.equal($('introReferenceTitle0').textContent,'Local sample (synthetic)');assert(!source.includes('introSourcesOpen')&&!source.includes('introCitation'));const readMarkup=source.match(/id="introViewport"[\s\S]*?id="introLoading"/)[0];assert(readMarkup.indexOf('introParagraph2')<readMarkup.indexOf('introSourceList'));assert(source.includes('<h3 class="music-source-heading">Source</h3>'));
 $('introReference0').onclick();assert.equal(state.view,'intro-sources');assert.equal($('introSourceDomain').textContent,'example.org');
 assert.equal($('introSourceLink').href,'https://example.org/local-introduction');assert.equal(run('introMock.loads'),sourceLoads);assert.equal(timers.size,0);
 $('introSourceBack').onclick();assert.equal(state.view,'intro-read');assert.equal($('introViewport').scrollTop,83);assert.equal(run('introMock.loads'),sourceLoads);
 $('introReference0').onclick();assert.equal(state.view,'intro-sources');state.track=1;run('renderIntro()');assert.equal($('introSourceDomain').textContent,'example.org');assert.equal(run('introMock.cache.length'),0);
 $('introSourceBack').onclick();assert.equal($('introReadSubject').textContent,tracks[0].artist);state.track=0;
 for(const url of ['https://example.org:443/path','https://127.0.0.1/path','HTTPS://example.org/path','https://example.org/'+ 'é'.repeat(190)]){ctx.auditUrl=url;assert.equal(run('safeIntroSource({url:auditUrl})'),null)}
 ctx.auditUrl='https://example.org/'+ 'é'.repeat(180);assert(run('safeIntroSource({url:auditUrl})'));
 assert.equal(run('safeIntroSource({url:"javascript:alert(1)"})'),null);assert.equal(run('safeIntroSource({url:"https://user:secret@example.org/x"})'),null);assert.equal(run('safeIntroSource({url:"https://example.org/ bad"})'),null);
 // Four retained source selectors, long labels, and exact link choice.
 run(`introMock.value.sources.push(...[2,3,4].map(i=>({title:'Résumé / 日本語 '+i,url:'https://example.org/reference/'+i})));introMock.value.paragraphSources[1]=[3]`);
 run('renderIntro()');assert(!$('introReference3').hidden);assert.equal($('introReferenceTitle3').textContent,'Résumé / 日本語 4');$('introReference3').onclick();assert.equal($('introSourceLink').href,'https://example.org/reference/4');assert(!$('introSourceTab3').hidden);
 $('introSourceTab1').onclick();assert.equal($('introSourceLink').href,'https://example.org/reference/2');$('introSourceBack').onclick();
 assert.equal(run('introMock.loads'),sourceLoads);run('introMock.value.sources.splice(1);introMock.value.paragraphSources[1]=[0];renderIntro()');
 // Keep already-open reading stable, but release all cached topics at track change.
 state.track=1;run('renderIntro()');assert.equal($('introReadSubject').textContent,tracks[0].artist);assert.equal(run('introMock.cache.length'),0);
 $('introReadBack').onclick();assert.equal($('introSubject').textContent,'Second track');rows[1].onclick();advance(1500);
 assert.equal($('introReadSubject').textContent,'Second track');assert.equal($('introMessage').textContent,'Release not identified');assert($('introViewport').hidden&&$('introRetry').hidden);
 state.track=0;run('renderIntro()');assert.equal($('introReadSubject').textContent,'Second track');$('introReadBack').onclick();assert.equal($('introSubject').textContent,tracks[0].title);rows[0].onclick();assert.equal($('introMessage').textContent,'Loading');advance(1500);$('introReadBack').onclick();$('introBack').onclick();
 // Pending results are canceled at change and never automatically restarted for stale context.
 reset('success');$('musicExplore').onclick();rows[2].onclick();state.track=1;run('renderIntro()');advance(5000);assert.equal($('introMessage').textContent,'Track changed');assert.equal(run('introMock.cache.length'),0);assert.equal(timers.size,0);$('introReadBack').onclick();$('introBack').onclick();
 state.track=1;tracks[1].artist=' ';$('musicExplore').onclick();assert(rows.every(r=>r.disabled));$('introBack').onclick();tracks[1].artist='New artist';tracks[1].title=' ';$('musicExplore').onclick();assert(rows[1].disabled);$('introBack').onclick();
 tracks[1].artist='';tracks[1].album='Supplied EP';$('musicExplore').onclick();assert(rows[1].disabled);$('introBack').onclick();
 tracks[1]={title:'Second track',artist:'New artist',album:''};state.track=0;
 // Every topic: first error, no automatic resubmission, explicit retry, saved hit.
 for(const topic of [0,1,2]){
  show('player');reset('retry');$('musicExplore').onclick();rows[topic].onclick();advance(1500);
  assert.equal($('introMessage').textContent,'Could not load introduction');assert(!$('introRetry').hidden&&$('introRetry').disabled);assert.equal(run('introMock.loads'),1);
  $('introRetry').onclick();assert.equal(run('introMock.loads'),1);advance(4999);assert($('introRetry').disabled);advance(1);assert(!$('introRetry').disabled);
  $('introRetry').onclick();assert.equal($('introMessage').textContent,'Loading');advance(1500);assert(!$('introViewport').hidden);assert.equal(run('introMock.loads'),2);
  $('introReadBack').onclick();rows[topic].onclick();assert.equal($('introFeedback').textContent,'Local sample | Saved');assert.equal(run('introMock.loads'),2);
 }
 for(const [scenario,delay,message,cooldown] of [['rate-limit',1500,'Try again later',3000],['timeout',30000,'Request timed out',5000]]){
  show('player');reset(scenario);$('musicExplore').onclick();rows[2].onclick();advance(delay);assert.equal($('introMessage').textContent,message);assert($('introRetry').disabled);
  advance(cooldown);assert(!$('introRetry').disabled);$('introRetry').onclick();advance(1500);assert(!$('introViewport').hidden);
 }
 show('player');reset('unavailable');$('musicExplore').onclick();rows[0].onclick();advance(1500);assert.equal($('introMessage').textContent,'Not enough information');assert($('introRetry').hidden);
 // Back while loading cancels; no timer, stale content or cached completion.
 show('player');reset('success');$('musicExplore').onclick();rows[0].onclick();$('introReadBack').onclick();advance(5000);assert.equal(state.view,'intro');assert.equal(timers.size,0);assert.equal(run('introMock.cache.length'),0);
 rows[0].onclick();advance(1500);assert.equal(run('introMock.loads'),2);assert(!$('introViewport').hidden);
 // Changing the external test scenario during reading never mixes old content.
 $('introMockScenario').value='retry';$('introMockScenario').onchange();assert.equal($('introMessage').textContent,'Loading');advance(1500);assert($('introViewport').hidden&&$('introRetry').disabled);
 active=false;show('player');run('renderIntro()');assert($('musicExplore').hidden);$('musicExplore').onclick();assert.equal(state.view,'player');active=true;
 assert(!/introNext|introPrevious|introCounter|music-intro-heading|People &amp; influences|The bigger picture/.test(source));
 const catalog=JSON.parse(fs.readFileSync(path.join(root,'ai/mock/catalog.json'),'utf8'));
 assert.deepEqual(JSON.parse(run('JSON.stringify(introMockCatalog)')),catalog);
 const generated=fs.readFileSync(path.join(root,'firmware/OrbitS3/intro_ai_mock_data.h'),'utf8');
 for(const p of catalog.paragraphs)for(const text of p)assert(generated.includes(JSON.stringify(text)));
 const css=source.match(/\/\* orbit-music-intro:start \*\/([\s\S]*?)\/\* orbit-music-intro:end \*\//)[1];assert(css.includes('var(--fg)')&&css.includes('var(--track)')&&css.includes('var(--accent)'));
}
console.log('PASS: both local intro UIs, Artist/Release/Song loading/ready/saved/error/retry, 429/deadline/no-match, cancel/frozen context, scroll, themes and no network/storage/audio hooks');
// Run the shared DOM marquee: preserve the text layer on unchanged polls, including paused playback.
for(const file of ['index.html','orbit-s3-preview.html']){
 const source=fs.readFileSync(path.join(root,file),'utf8');
 const block=source.match(/\/\/ orbit-metadata-motion:start([\s\S]*?)\/\/ orbit-metadata-motion:end/)[1];
 const setter=source.match(/function metadataText\(id,value\)\{[^\n]+/)[0];
 let asleep=false,restarts=0;const nodes={};
 function element(){const set=new Set();let content='';const el={className:'',children:[],attrs:{},fullWidth:900,clientWidth:200,
  get textContent(){return this.children.length?this.children.map(c=>c.textContent).join(''):content},set textContent(v){content=v;this.children=[]},
  classList:{add(x){if(x==='metadata-overflow'&&!set.has(x))restarts++;set.add(x)},remove:x=>set.delete(x),contains:x=>set.has(x)||el.className.split(' ').includes(x)},
  style:{values:{},setProperty(k,v){this.values[k]=v}},setAttribute(k,v){this.attrs[k]=v},
  get firstElementChild(){return this.children[0]},get offsetWidth(){return this.fullWidth},
  append(...c){this.children.push(...c)},replaceChildren(...c){this.children=c},
  querySelector(sel){return sel==='.metadata-value'?this.firstElementChild?.firstElementChild:null}
 };return el}
 for(const id of ['trackTitle','trackArtist','introSubject','introContext','introReadSubject']){nodes[id]=element();nodes[id].textContent='A long captured name / 日本語'}
 const state={view:'intro',standby:false,artworkZoomed:false,playing:false},ctx={$,state,document:{createElement:element}};
 function $(id){return id==='screen'?{classList:{contains:()=>asleep}}:nodes[id]}
 vm.createContext(ctx);vm.runInContext(block+'\n'+setter,ctx);const run=()=>vm.runInContext('renderMetadataMotion()',ctx),get=id=>vm.runInContext('metadataMotion.get('+JSON.stringify(id)+')',ctx);
 run();assert.equal(restarts,2);const current=get('introContext');assert.equal(current.runner.style.values['--metadata-end'],'-932px');assert.equal(current.runner.children[1].attrs['aria-hidden'],'true');
 const primary=current.runner.firstElementChild;vm.runInContext("metadataText('introContext','A long captured name / 日本語')",ctx);run();assert.equal(restarts,2);assert.equal(get('introContext'),current);assert.equal(current.runner.firstElementChild,primary);
 // No frame-by-frame measurement of animated bounds, no layout-based text-indent animation.
 assert(!block.includes('getBoundingClientRect')&&!block.includes('createRange')&&!block.includes('textIndent'));
 primary.textContent+=' avec invités';primary.fullWidth=1100;run();assert.equal(restarts,3);assert.equal(get('introContext').runner.style.values['--metadata-end'],'-1132px');
 vm.runInContext("metadataText('introContext','Replacement song author')",ctx);run();assert.notEqual(get('introContext').runner,current.runner);assert.equal(get('introContext').runner.firstElementChild.textContent,'Replacement song author');
 nodes.introSubject.firstElementChild.firstElementChild.fullWidth=75;run();assert.equal(get('introSubject'),undefined);assert(!nodes.introSubject.classList.contains('metadata-overflow'));
 asleep=true;run();assert.equal(get('introContext'),undefined);asleep=false;run();assert(get('introContext'));
 state.view='intro-read';run();assert.equal(get('introContext'),undefined);assert(get('introReadSubject'));state.view='player';run();assert.equal(get('introReadSubject'),undefined);assert(get('trackArtist'));state.standby=true;run();assert.equal(get('trackArtist'),undefined);
 assert(source.includes('font-size:16px;line-height:20px'));assert(source.includes('linear infinite'));assert(source.includes('@keyframes metadata-marquee'));
}
console.log('PASS: shared continuous home/intro/reader marquee, intrinsic measurements, stable text layers/paused polls, short/hidden/idle/standby gating; both previews');
