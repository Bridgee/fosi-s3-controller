// Simulated display idle only; media timers/state continue while the screen is dark.
function installScreenIdle(screen,timeoutMs,keyboardRoot) {
 let timer,asleep=false,dimmed=false,held=false,wakeGesture=false,suppressClick=false;
 const stop=e=>{e.preventDefault();e.stopImmediatePropagation()};
 const wake=()=>{asleep=dimmed=false;screen.classList.remove('display-asleep');screen.classList.remove('display-dimmed');renderMetadataMotion();renderClockPreview();renderWeatherPreview();if(typeof renderLyricsPreview==='function')renderLyricsPreview()};
 const arm=()=>{clearTimeout(timer);if(!held&&state.view!=='lyrics'&&state.view!=='clock-alert')timer=setTimeout(()=>{settlePageTransition();if(state.view==='blank-up'){dimmed=true;screen.classList.add('display-dimmed')}else{asleep=true;screen.classList.add('display-asleep')}renderMetadataMotion()},timeoutMs)};
 screen.addEventListener('pointerdown',e=>{
  held=true;clearTimeout(timer);wakeGesture=asleep||dimmed;suppressClick=wakeGesture;
  if(wakeGesture){wake();stop(e)}
 },true);
 screen.addEventListener('pointermove',e=>{if(held&&wakeGesture)stop(e)},true);
 for(const type of ['pointerup','pointercancel'])window.addEventListener(type,e=>{
  if(!held)return;held=false;arm();
  if(wakeGesture){wakeGesture=false;stop(e)}
 },true);
 screen.addEventListener('click',e=>{if(suppressClick){suppressClick=false;stop(e)}},true);
 screen.addEventListener('orbit-preview-wake',()=>{wake();arm()});
 screen.addEventListener('orbit-view-change',()=>{if(dimmed)wake();arm()});screen.addEventListener('orbit-clock-alert',()=>{wake();arm()});
 screen.addEventListener('wheel',e=>{if(asleep||dimmed){wake();stop(e)}arm()},{capture:true,passive:false});
 window.addEventListener('keydown',e=>{if(!keyboardRoot.contains(e.target))return;if(asleep||dimmed){wake();stop(e)}arm()},true);
 arm();
}
