const lyricsMock=createLyricsPreview();let lyricsOpenKey='',lyricCursor=-999,lyricDocument=null,lyricsConfigured=false;
const lyricOriginalMetadata=tracks.map(t=>({title:t.title,album:t.album}));
function previewLyricTrack(){const t=tracks[state.track];return t&&state.input==='network'&&state.networkActive?{title:t.title||'',artist:t.artist||'',album:t.album||'',duration:t.duration}:null}
function lyricContextActive(){return canPlay()&&state.input==='network'&&!state.standby&&state.previewWifi!==false&&previewParams.get('wifi')!=='offline'&&previewParams.get('link')!=='offline'}
function hasPreviewLyrics(){return lyricsMock.active&&lyricsMock.state==='ready'&&lyricsMock.document?.key===lyricsMock.key&&lyricContextActive()}
function clearLyricMotion(){for(let i=0;i<5;i++)for(const animation of $('lyricRow'+i).getAnimations?.()||[])animation.cancel();lyricCursor=-999;lyricDocument=null}
function renderLyricsTools(){
 const tools=$('lyricsMockTools');if(tools.hidden)return;
 const t=tracks[state.track],matched=hasPreviewLyrics();
 const status=!lyricsMock.active?'Track information unavailable':lyricsMock.state==='ready'?'Matched':lyricsMock.state==='matching'?'Matching…':lyricsMock.state==='error'?'Temporary error · Retry in '+Math.max(0,Math.ceil((lyricsMock.retryAt-performance.now())/1000))+'s':'No synced lyrics';
 $('lyricsMockStatus').textContent=status+' · '+lyricsMock.attempts+' sample lookups · '+lyricsMock.hits+' cache hits';
 $('lyricsMockPlay').textContent=state.playing?'Pause':'Play';$('lyricsMockPlay').disabled=$('lyricsMockNext').disabled=!lyricContextActive();
 $('lyricsMockSeek').disabled=!lyricContextActive();$('lyricsMockSeek').max=t?.duration||0;$('lyricsMockSeek').value=state.elapsed;
 $('lyricsMockTime').textContent=format(state.elapsed)+' / '+format(t?.duration||0);
 $('lyricsMockOpen').disabled=!matched;
}
function renderLyricsPreview(){
 lyricsMock.observe(previewLyricTrack(),lyricContextActive());
 const ready=hasPreviewLyrics();$('lyricsEntry').hidden=!ready||state.view!=='player'||state.artworkZoomed;
 renderLyricsTools();if(state.view!=='lyrics')return;
 if(!ready||lyricsOpenKey!==lyricsMock.key){clearLyricMotion();show('player');return}
 if($('screen').classList.contains('display-asleep')||document.hidden)return;
 const doc=lyricsMock.document,index=currentPreviewLyric(doc.lines,state.elapsed*1000);
 if(index===lyricCursor&&doc===lyricDocument)return;
 const center=Math.max(0,index),direction=lyricDocument===doc&&lyricCursor>=0?(index===lyricCursor+1?1:index===lyricCursor-1?-1:0):0;
 const oldY=direction?$('lyricRow'+(direction>0?3:1)).offsetTop:0,rows=[];
 for(let i=0;i<5;i++){
  const row=$('lyricRow'+i),line=doc.lines[center+i-2];for(const animation of row.getAnimations?.()||[])animation.cancel();
  row.textContent=line?line.text.normalize('NFC'):'';row.classList.toggle('current',i===2&&index>=0);row.style.opacity=i===2&&index>=0?'1':i===0||i===4?String(96/255):String(160/255);row.classList.remove('small');if(row.offsetHeight>300)row.classList.add('small');rows.push(row);
 }
 const heights=rows.map(row=>Math.max(26,row.offsetHeight)),y=[0,0,(320-heights[2])/2,0,0];
 for(let i=1;i>=0;i--)y[i]=y[i+1]-14-heights[i];for(let i=3;i<5;i++)y[i]=y[i-1]+heights[i-1]+14;
 rows.forEach((row,i)=>row.style.top=y[i]+'px');
 if(direction&&!matchMedia('(prefers-reduced-motion: reduce)').matches)for(const row of rows)row.animate([{transform:'translateY('+(oldY-y[2])+'px)'},{transform:'translateY(0)'}],{duration:240,easing:'ease-out'});
 lyricCursor=index;lyricDocument=doc;
}
$('lyricsEntry').onclick=()=>{if(!hasPreviewLyrics())return;lyricsOpenKey=lyricsMock.key;clearLyricMotion();show('lyrics')};
$('page-lyrics').onclick=()=>show('player');
$('screen').addEventListener('orbit-view-change',()=>{if(state.view!=='lyrics')clearLyricMotion()});
function configureLyricsMocks(params){
 $('lyricsMockTools').addEventListener('pointerdown',()=>$('screen').dispatchEvent(new Event('orbit-preview-wake')));
 const visible=params.get('lyricsTools')==='1';$('lyricsMockTools').hidden=!visible;document.documentElement.classList.toggle('lyrics-mock-panel',visible);
 function scenario(value){
  clearLyricMotion();if(state.view==='lyrics')show('player');
  tracks.forEach((t,i)=>Object.assign(t,lyricOriginalMetadata[i]));
  // Preserve caller-specified long/non-Latin metadata; only modify the fictional
  // standard sample for the explicit annotation test.
  if(value==='notes'&&!params.has('metadata'))tracks[0].title+=' (Remastered 2009)';
  lyricsMock.reset(value==='notes'||value==='matched'?'auto':value==='none'?'not-found':value);$('lyricsMockScenario').value=value==='matched'?'auto':value==='none'?'not-found':value;
  render();
 }
 // Initial configuration must not replace metadata set by other preview tools.
 tracks.forEach((t,i)=>Object.assign(lyricOriginalMetadata[i],{title:t.title,album:t.album}));
 const initial=params.get('lyrics')||'auto';lyricsMock.reset(initial==='matched'?'auto':initial==='none'?'not-found':initial==='notes'?'auto':initial);
 if(initial==='notes'&&!params.has('metadata'))tracks[0].title+=' (Remastered 2009)';
 $('lyricsMockScenario').value=initial==='matched'?'auto':initial==='none'?'not-found':initial;
 $('lyricsMockScenario').onchange=()=>scenario($('lyricsMockScenario').value);
 $('lyricsMockReset').onclick=()=>scenario($('lyricsMockScenario').value);
 $('lyricsMockPlay').onclick=()=>{togglePlay();renderLyricsTools()};$('lyricsMockNext').onclick=()=>changeTrack(1);
 $('lyricsMockSeek').oninput=()=>{if(!lyricContextActive())return;const value=Number($('lyricsMockSeek').value),duration=tracks[state.track]?.duration;if(Number.isFinite(value)&&duration>0){state.elapsed=Math.max(0,Math.min(duration,value));render()}};
 $('lyricsMockOpen').onclick=()=>$('lyricsEntry').onclick();lyricsConfigured=true;
}
setInterval(()=>{
 if(!lyricsConfigured)return;const changed=lyricsMock.observe(previewLyricTrack(),lyricContextActive());
 // Matching can finish while the simulated display is asleep; repaint on wake.
 if(!$('screen').classList.contains('display-asleep')&&!document.hidden&&(changed||state.view==='lyrics'))renderLyricsPreview();
 else if(!document.hidden)renderLyricsTools();
},100);
