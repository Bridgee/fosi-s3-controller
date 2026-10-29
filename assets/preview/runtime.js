// One local-only behavior layer shared by both standalone previews.
// No fetch/XHR, provider key, device command or browser-to-firmware connection.
const previewOptions=new URLSearchParams(location.search);
const previewScreen=$('screen');
const previewBluetoothStatus=previewOptions.get('bluetooth')||'connected';
const previewBluetoothLabel={connected:'Connected',disconnected:'Not connected',pairing:'Pairing mode',searching:'Searching',unknown:'Unknown'}[previewBluetoothStatus]||'Unknown';
Object.assign(state,{previewLink:previewOptions.get('link')||previewOptions.get('status')||'connected',previewWifi:previewOptions.get('wifi')!=='offline',bluetoothStatus:previewBluetoothStatus,bluetoothConnected:previewBluetoothStatus==='connected',bluetoothControl:previewOptions.get('btControl')==='1',volumeReadOnly:previewOptions.get('readOnly')==='1'});
const previewDisconnected=()=>state.previewLink==='offline'||!state.previewWifi;
const previewMusicReady=canPlay;
canPlay=()=>!previewDisconnected()&&previewMusicReady();
const previewBluetoothPlaying=()=>!state.standby&&!previewDisconnected()&&state.input==='bluetooth'&&state.bluetoothConnected;
const previewMediaControl=()=>canPlay()||(previewBluetoothPlaying()&&state.bluetoothControl);
const previewSetVolume=setVolume;
setVolume=value=>{if(state.standby||previewDisconnected()||state.volumeReadOnly)return;previewSetVolume(value)};
toggleMute=()=>{if(state.standby||previewDisconnected())return;state.muted=!state.muted;render();notice('Mute')};
togglePlay=()=>{if(!previewMediaControl())return;state.playing=!state.playing;render();notice('Playback')};
const previewChangeTrack=changeTrack;
changeTrack=delta=>{if(!previewMediaControl())return;if(canPlay())previewChangeTrack(delta);else{state.elapsed=0;render();notice('Playback')}};
function nativeGlyph(selector,key,assetOverride){
 const spec=previewContract.glyphs[key];if(!spec)return;
 for(const element of previewScreen.querySelectorAll(selector)){
  element.classList.add('native-font-icon');element.style.width=spec.width+'px';element.style.height=spec.height+'px';
  const uri='url("'+(assetOverride||spec.path)+'")';element.style.maskImage=uri;element.style.webkitMaskImage=uri;
 }
}
const previewStatusMark=document.createElement('span');previewStatusMark.className='native-status-icon';previewStatusMark.setAttribute('aria-hidden','true');$('deviceStatus').append(previewStatusMark);
let previewStatusMode='',previewStatusAt=0;
function syncNativeStatus(){
 const mode=$('deviceStatus').dataset.state;
 if(mode!==previewStatusMode){previewStatusMode=mode;previewStatusAt=performance.now()}
 const frame=mode==='waiting'?Math.floor((performance.now()-previewStatusAt)/previewContract.statusFrameMs)%previewContract.statusFrames:0;
 const uri='url("assets/preview/status-'+mode+'_'+frame+'.svg")';previewStatusMark.style.maskImage=uri;previewStatusMark.style.webkitMaskImage=uri;
}
steadyFooterStatus=()=>footerStatus(state.standby?'sleep':previewDisconnected()?'offline':state.previewLink==='waiting'||state.previewLink==='connecting'?'waiting':['confirmed','error','limited','sleep','demo'].includes(state.previewLink)?state.previewLink:'connected',state.standby?'Standby':previewDisconnected()?'Not connected':'Connected');
const previewFooterStatus=footerStatus;
footerStatus=(...args)=>{previewFooterStatus(...args);syncNativeStatus()};
setInterval(()=>{if(!document.hidden&&!previewScreen.classList.contains('display-asleep')&&!pageTransitionActive&&!state.artworkZoomed&&state.view==='player')syncNativeStatus()},previewContract.statusFrameMs);
const previewSourceRender=renderIntroSource;
renderIntroSource=()=>{
 previewSourceRender();const ref=introMock.state==='ready'?introMock.value?.sources[introSourceIndex]:null;
 const image=ref&&previewContract.sourceQr[ref.url],link=$('introSourceLink');link.classList.toggle('qr-source',!!image);
 if(image){let img=link.querySelector('img');if(!img){img=document.createElement('img');img.alt='';link.replaceChildren(img)}if(img.getAttribute('src')!==image)img.setAttribute('src',image);link.setAttribute('aria-label','Open source: '+ref.url)}
};
function syncNativeAbout(){
 const values=$('page-about').querySelectorAll('dd');
 const network=state.previewWifi,offline=previewDisconnected(),physical=['line','hdmi','optical'].includes(state.input);
 const row=['192.0.2.33','192.0.2.23',state.previewLink==='connecting'?'Searching':offline?network?'S3 offline':'Wi-Fi offline':state.previewLink==='waiting'?'Reconnecting':'Connected',state.standby?'Standby':offline?'Unknown':'Online',network?'Good':'Not connected',state.bluetoothConnected?'Connected':state.bluetoothStatus==='connected'?'Not connected':previewBluetoothLabel,'Unknown',previewContract.firmwareVersion];
 if(!network)row[0]='Not connected';if(offline)row[1]='Not found';
 values.forEach((value,i)=>value.textContent=row[i]||'');
 $('aboutInputKey').textContent=physical?({line:'Line In signal',hdmi:'HDMI signal',optical:'Optical signal'}[state.input]):'Input';
 $('aboutInputValue').textContent=offline||physical?'Unknown':state.input==='bluetooth'?'Bluetooth':state.input==='network'?'Network':'Unknown';
 $('aboutStatus').textContent=state.previewLink==='error'?'Unable to update':state.previewLink==='limited'?'Some controls unavailable':'';
}
const previewRender=render;
render=()=>{
 previewRender();syncNativeAbout();
 const blocked=state.standby||previewDisconnected(),bt=previewBluetoothPlaying();
 $('minus').disabled=$('plus').disabled=blocked||state.volumeReadOnly;$('mute').disabled=$('muteSmall').disabled=blocked;
 $('volumeLimitHint').textContent=state.volumeReadOnly?'Read only':state.limitReached?'Volume limit reached':'';
 if(previewDisconnected()){$('chipValue').textContent=$('volumeNumber').textContent='--'}
 for(const button of $('transport').querySelectorAll('button'))button.disabled=!previewMediaControl();
 $('transport').classList.toggle('disabled',!previewMediaControl());
 if(bt){$('vinylDisc').style.animationPlayState=state.playing&&state.view==='player'?'running':'paused';$('vinyl').classList.toggle('record-playing',state.playing);$('vinyl').setAttribute('aria-label',state.playing?'No artwork. Record spinning.':'No artwork. Record stopped.')}
 for(const button of previewScreen.querySelectorAll('[data-input],[data-output]'))button.disabled=blocked;
 $('standbyS3').disabled=blocked;
 nativeGlyph('#volumeChip>svg',state.muted?'mute18':'volume18');nativeGlyph('#muteSmall>svg',state.muted?'mute26':'volume26',state.muted?'assets/preview/mute-26-slash.svg':null);
 nativeGlyph('#previous>svg','prev24');nativeGlyph('#next>svg','next24');nativeGlyph('#play>svg',state.playing?'pause24':'play24');
 nativeGlyph('.back>svg','back18');nativeGlyph('#themeOpen>svg','theme24');nativeGlyph('#standbyS3>svg','power24');nativeGlyph('[data-input="bluetooth"]>svg','bluetooth18');
 syncNativeStatus();
};
// Clock and weather share one simulated location/timezone; neither looks up IP.
function clockPreviewReady(){return state.previewWifi!==false&&!['offline','locating','syncing'].includes(previewOptions.get('clock'))}
const previewClockRender=renderClockPreview;
renderClockPreview=()=>{
 previewClockRender();if(state.view!=='blank-up')return;
 const mode=!state.previewWifi?'offline':previewOptions.get('clock');
 if(['offline','locating','syncing'].includes(mode)){
  pixelText('pixelDate','');pixelText('pixelTime','--:--');pixelText('pixelSeconds','--');pixelText('pixelZone','');pixelText('pixelStatus',mode==='offline'?'CONNECT TO WI-FI':mode==='locating'?'FINDING LOCATION':'SYNCING TIME');
 }
};

const previewLaunchRender=renderLaunch;
renderLaunch=()=>{previewLaunchRender();if(previewDisconnected()){$('launchAppleMusic').disabled=true;$('launchStatus').textContent=state.previewWifi?'S3 unavailable':'Wi-Fi unavailable'}};
const previewLaunch=simulateLaunch;
simulateLaunch=()=>{if(!previewDisconnected())previewLaunch()};
const previewMeta=document.createElement('div');previewMeta.className='preview-meta';previewMeta.textContent='Firmware '+previewContract.firmwareVersion+' · Local simulation';previewScreen.closest('.stage').append(previewMeta);
$('muteSmall').onclick=$('mute').onclick=toggleMute;
$('play').onclick=togglePlay;$('launchAppleMusic').onclick=simulateLaunch;
previewScreen.dataset.firmware=previewContract.firmwareVersion;
for(const [id,rect] of Object.entries(previewContract.rects)){const el=$(id);el.style.left=rect[0]+'px';el.style.top=rect[1]+'px';el.style.width=rect[2]+'px';if(rect.length===4)el.style.height=rect[3]+'px'}
steadyFooterStatus();render();
