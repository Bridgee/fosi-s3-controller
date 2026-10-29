// Bounded offline mirror of the native lyric lookup/acceptance policy.
// All records are original local samples. No provider or device client exists.
const lyricPolicy={debounce:1500,gap:500,retry:60000,negativeTtl:1800000,positiveEntries:2,negativeEntries:4,maxLines:256,textBytes:24576,lineBytes:240};
const lyricBytes=value=>new TextEncoder().encode(value).length;
function lyricValidText(value,multiline=false){return typeof value==='string'&&!/[\ud800-\udfff]/u.test(value)&&!(multiline?/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f-\u009f\u200b-\u200f\u202a-\u202e\u2060-\u206f]/u:/[\u0000-\u0008\u000a-\u001f\u007f-\u009f\u200b-\u200f\u202a-\u202e\u2060-\u206f]/u).test(value)}
function lyricName(value){return lyricValidText(value)&&lyricBytes(value)>0&&lyricBytes(value)<159?value.normalize('NFC').replace(/[A-Z]/g,c=>c.toLowerCase()).replace(/[ \t]+/g,' ').replace(/^ | $/g,''):''}
function lyricSameName(a,b){const x=lyricName(a);return !!x&&x===lyricName(b)}
function lyricRemasterNote(value){
 if(!value||lyricBytes(value)>100)return false;let marker=false;
 for(const word of value.replace(/[A-Z]/g,c=>c.toLowerCase()).match(/[a-z]+|[0-9]+|重制版|重製版|重制|重製|リマスタリング|リマスター|年|[^\s]/gu)||[]){
  if(['remaster','remastered','remastering','重制版','重製版','重制','重製','リマスタリング','リマスター'].includes(word)){marker=true;continue}
  if(['digital','digitally','version','the','年','-', '.', ':'].includes(word)||/^[0-9]{4}$/.test(word)&&Number(word)>=1900&&Number(word)<=2099)continue;
  return false;
 }return marker;
}
function lyricVersionNote(value){
 if(lyricBytes(value)>100)return true;const text=value.replace(/[A-Z]/g,c=>c.toLowerCase());
 const versions=new Set(['live','liveat','remix','mix','edit','acoustic','acoustique','instrumental','karaoke','demo','extended','part','pt','session','rehearsal','alternate','take','rerecorded','rerecording','feat','featuring','ft','vivo','directo','dub']);
 if((text.match(/[a-z]+/g)||[]).some(w=>versions.has(w)||w.startsWith('live')||w.startsWith('feat')||w.endsWith('mix')||w.endsWith('edit')))return true;
 if(/现场|現場|ライブ|アコースティック|カラオケ|混音|伴奏|纯音乐|純音樂|中文版|日文版|国语版|國語版|粤语版|粵語版/u.test(text))return true;
 return text.includes('version')&&!lyricRemasterNote(value);
}
function lyricStripNotes(source){
 if(typeof source!=='string'||lyricBytes(source)>=159)return source;
 const pairs={'(':')','[':']','{':'}','（':'）','【':'】','「':'」','〈':'〉','《':'》'};let out='',changed=false;
 for(let i=0;i<source.length;){const close=pairs[source[i]];
  if(close){const end=source.indexOf(close,i+1);if(end>=0&&!lyricVersionNote(source.slice(i+1,end))){changed=true;i=end+1;if(out&&!out.endsWith(' '))out+=' ';continue}}
  const char=source[i],rest=source.slice(i+1);
  if(i>0&&((char==='-'&&(source[i-1]===' '||source[i+1]===' '||lyricRemasterNote(rest)))||char==='–'||char==='—')&&!lyricVersionNote(rest)){changed=true;break}
  if(i>0&&[' ', ',', ':'].includes(char)&&lyricRemasterNote(rest)){changed=true;break}
  out+=source[i++];
 }
 if(changed)out=out.replace(/[ \t-]+$/g,'');return out||source;
}
function lyricSameTitle(a,b){return lyricSameName(a,b)||lyricSameName(lyricStripNotes(a),lyricStripNotes(b))}
function lyricTrackValid(track){return !!track&&[track.title,track.artist,track.album].every((v,i)=>lyricValidText(v)&&lyricBytes(v)<159&&(i===2||!!lyricName(v)))&&Number.isInteger(track.duration)&&track.duration>=1&&track.duration<=3600}
function lyricStamp(value){const m=/^(\d{1,3}):([0-5]\d)(?:[.:](\d{1,3}))?$/.exec(value);return m?(Number(m[1])*60+Number(m[2]))*1000+Number((m[3]||'').padEnd(3,'0')):null}
function parsePreviewLrc(source,track,signature=false){
 if(!lyricTrackValid(track)||!lyricValidText(source,true)||!source||lyricBytes(source)>32768)return null;
 let rows=[],used=0,nonempty=0,offset=0,haveOffset=false,haveLength=false,haveAlbum=false;
 for(const raw of source.replace(/^\ufeff/u,'').split('\n')){let line=raw.replace(/\r$/,''),pos=0,stamps=[];
  while(line[pos]==='['){const end=line.indexOf(']',pos+1);if(end<0)return null;const tag=line.slice(pos+1,end),at=lyricStamp(tag);pos=end+1;
   if(at!==null){if(stamps.length>=8)return null;stamps.push(at);continue}if(/^\d/.test(tag))return null;
   const colon=tag.indexOf(':');if(colon<0)continue;const key=tag.slice(0,colon).toLowerCase(),value=tag.slice(colon+1);
   if(key==='offset'){if(haveOffset||!/^[ \t]*[+-]?\d+$/.test(value)||value.length>7||Math.abs(Number(value))>10000)return null;offset=Number(value);haveOffset=true}
   else if(key==='length'){const n=lyricStamp(value);if(haveLength||n===null||Math.abs(n-track.duration*1000)>2000)return null;haveLength=true}
   else if(['ti','ar','al'].includes(key)){const expected=track[{ti:'title',ar:'artist',al:'album'}[key]];if(lyricBytes(value)>=159||key==='ar'&&!lyricSameName(value,expected)||key!=='ar'&&!(key==='al'&&!expected)&&!lyricSameTitle(value,expected))return null;if(key==='al')haveAlbum=true}
  }
  if(!stamps.length)continue;
  let text=line.slice(pos).replace(/^[ \t]+|[ \t]+$/g,'').replace(/<([^<>]{1,16})>/g,(all,tag)=>lyricStamp(tag)!==null?'':all).replace(/\t/g,' ');
  if(lyricBytes(text)>lyricPolicy.lineBytes)return null;
  for(const at of stamps){used+=lyricBytes(text)+1;if(at>track.duration*1000+2000||rows.length>=lyricPolicy.maxLines||used>lyricPolicy.textBytes)return null;rows.push({at,text});if(text)nonempty++}
 }
 if(signature&&(!haveLength||track.album&&!haveAlbum)||rows.length<2||nonempty<2)return null;
 rows=rows.map(row=>({...row,at:Math.max(0,row.at-offset)})).sort((a,b)=>a.at-b.at);const merged=[];
 for(const row of rows){const old=merged.at(-1);if(old&&old.at===row.at){if(!row.text||row.text===old.text)continue;if(!old.text){old.text=row.text;continue}const text=old.text+'\n'+row.text;used+=lyricBytes(text)+1;if(lyricBytes(text)>lyricPolicy.lineBytes||used>lyricPolicy.textBytes)return null;old.text=text}else merged.push({...row})}
 return merged.length>=2?merged:null;
}
function matchPreviewLyrics(provider,records,track,relaxed=false){
 const sameTitle=relaxed?lyricSameTitle:lyricSameName;
 if(provider==='primary'){
  for(const r of records){if(r.provider!=='primary'||typeof r.duration!=='number'||!Number.isFinite(r.duration)||Math.abs(r.duration-track.duration)>2||r.instrumental!==false||!sameTitle(r.trackName,track.title)||!lyricSameName(r.artistName,track.artist)||track.album&&!sameTitle(r.albumName,track.album))continue;
   const lines=parsePreviewLrc(r.syncedLyrics,track);if(lines)return {provider,lines};
  }return null;
 }
 const entries=records.filter(r=>r.provider==='fallback');if(entries.length>12)return null;let found=null;
 for(const r of entries){if(!sameTitle(r.title,track.title)||!lyricSameName(r.artist,track.artist))continue;const lines=parsePreviewLrc(r.lyrics,track,true);if(!lines)continue;if(found)return null;found={provider,lines}}
 return found;
}
function currentPreviewLyric(lines,positionMs){let low=0,high=lines.length;while(low<high){const mid=(low+high)>>>1;if(lines[mid].at<=positionMs)low=mid+1;else high=mid}return low-1}
function createLyricsPreview(clock=()=>performance.now(),records=lyricsPreviewCatalog.records){
 const m={state:'idle',key:'',generation:0,document:null,job:null,active:false,cache:[],missing:[],attempts:0,hits:0,scenario:'auto',retryAt:0};
 const key=t=>JSON.stringify([t.title,t.artist,t.album,t.duration]);
 function queue(track,retried=false){const alternate={...track,title:lyricStripNotes(track.title),album:lyricStripNotes(track.album)},relax=alternate.title!==track.title||alternate.album!==track.album;
  m.job={generation:m.generation,key:m.key,track:{...track},alternate,steps:relax?[['primary',false],['primary',true],['fallback',true]]:[['primary',false],['fallback',false]],step:0,at:clock()+lyricPolicy.debounce,retried};m.state='matching';m.document=null;
 }
 m.observe=(track,active=true)=>{m.active=active&&lyricTrackValid(track);const next=lyricTrackValid(track)?key(track):'';
  if(next!==m.key){m.generation++;m.key=next;m.document=null;m.job=null;m.state='idle';m.retryAt=0;if(next)queue(track)}
  if(!m.active)return false;return m.tick();
 };
 m.tick=()=>{
  if(!m.active||!m.job)return false;const job=m.job,now=clock();if(job.generation!==m.generation||job.key!==m.key){m.job=null;return false}
  if(now<job.at)return false;
  if(job.step===0){const cached=m.cache.find(c=>c.key===m.key);if(cached){m.hits++;m.document=cached.document;m.state='ready';m.job=null;return true}
   m.missing=m.missing.filter(c=>now>=c.at&&now-c.at<lyricPolicy.negativeTtl);if(m.missing.some(c=>c.key===m.key)){m.hits++;m.state='unavailable';m.job=null;return true}}
  if(m.scenario==='pending')return false;
  const [provider,relaxed]=job.steps[job.step],lookup=relaxed?job.alternate:job.track;m.attempts++;
  const fault=m.scenario==='error'&&!job.retried;let candidates=records;
  if(m.scenario==='not-found'||fault)candidates=[];
  if(m.scenario==='plain')candidates=records.map(r=>({...r,syncedLyrics:'Local plain text without timestamps',lyrics:'Local plain text without timestamps'}));
  if(m.scenario==='mismatch')candidates=records.map(r=>({...r,duration:trackDuration(r)+10,artistName:'Different artist',artist:'Different artist'}));
  const document=matchPreviewLyrics(provider,candidates,lookup,relaxed);
  if(document){m.document={...document,key:m.key,track:job.track};m.cache.push({key:m.key,document:m.document});if(m.cache.length>lyricPolicy.positiveEntries)m.cache.shift();m.state='ready';m.job=null;return true}
  if(++job.step<job.steps.length){job.at=now+lyricPolicy.gap;return false}
  if(fault){m.state='error';m.retryAt=now+lyricPolicy.retry;queue(job.track,true);m.job.at=m.retryAt;m.state='error'}
  else{m.state='unavailable';m.missing.push({key:m.key,at:now});if(m.missing.length>lyricPolicy.negativeEntries)m.missing.shift();m.job=null}
  return true;
 };
 function trackDuration(record){return typeof record.duration==='number'?record.duration:0}
 m.reset=(scenario='auto')=>{m.generation++;m.key='';m.document=null;m.job=null;m.state='idle';m.cache=[];m.missing=[];m.scenario=['auto','not-found','plain','mismatch','error','pending'].includes(scenario)?scenario:'auto';m.attempts=m.hits=0;m.retryAt=0};
 return m;
}
