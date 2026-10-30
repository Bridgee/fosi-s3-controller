// Browser mirror of the native Engine policy, with a clock and samples only.
// No provider client, network, storage, audio or device hooks.
function createIntroMock(clock=()=>performance.now()){
 const m={state:'idle',value:null,fromCache:false,loads:0,hits:0,cache:[],scenario:'success',generation:0,blocked:0,reason:'rate_limited',job:null,context:'',failedTopics:0};
 const utf8Bytes=s=>new TextEncoder().encode(s).length;
 const valid=s=>typeof s==='string'&&!/[\u0000-\u001f\u007f-\u009f\u2028-\u202e\u2066-\u2069]/u.test(s)&&!/[\ud800-\udfff]/u.test(s);
 m.remaining=()=>Math.max(0,m.blocked-clock());
 m.cancel=()=>{m.generation++;m.state='canceled';m.job=null;m.value=null;m.fromCache=false};
 m.trackChanged=()=>{if(m.state==='pending')m.cancel();m.cache=[];m.context='';m.failedTopics=0;m.fromCache=false};
 m.reset=scenario=>{m.cancel();m.scenario=['success','retry','rate-limit','timeout','unavailable','disabled'].includes(scenario)?scenario:'success';m.state='idle';m.cache=[];m.blocked=0;m.loads=m.hits=m.failedTopics=0;m.context=''};
 m.begin=(data,topic)=>{
  if(m.state==='pending')return 'busy';
  m.generation++;m.value=null;m.fromCache=false;
  if(m.scenario==='disabled'){m.job=null;return m.state='disabled'}
  const s={title:data.title||'',artist:data.artist||'',album:data.album||'',duration:Number.isInteger(data.duration)?data.duration:0};
  if(!Number.isInteger(topic)||topic<0||topic>2||![s.title,s.artist,s.album].every(valid)||utf8Bytes(s.title)>=256||utf8Bytes(s.artist)>=192||utf8Bytes(s.album)>=256||s.duration<0||s.duration>86400||!s.artist.trim()||(topic===2&&!s.title.trim())||(topic===1&&!s.title.trim()&&!s.album.trim()))return m.state='invalid_metadata';
  const now=clock(),key=JSON.stringify([introMockPolicy.model,introMockPolicy.version,'en',topic,s]);
  const context=JSON.stringify(s);if(m.context!==context){m.cache=[];m.context=context;m.failedTopics=0}
  m.cache=m.cache.filter(e=>now>=e.at&&(e.state==='ready'||now-e.at<introMockPolicy.missTtl));
  const cached=m.cache.find(e=>e.key===key);
  if(cached){cached.used=now;m.state=cached.state;m.value=cached.value;m.fromCache=true;m.hits++;return m.state}
  if(m.remaining()>0)return m.state=m.reason;
  const fail=['retry','rate-limit','timeout'].includes(m.scenario)&&!(m.failedTopics&(1<<topic));if(fail)m.failedTopics|=1<<topic;
  m.loads++;m.blocked=now+introMockPolicy.gap;m.reason='rate_limited';m.state='pending';
  m.job={generation:m.generation,at:now,key,s,topic,fail,scenario:m.scenario};return m.state;
 };
 m.tick=()=>{
  if(m.state!=='pending'||!m.job)return false;
  const j=m.job,now=clock();if(j.generation!==m.generation)return false;
  const fail=(state,wait)=>{m.state=state;m.value=null;m.job=null;m.blocked=now+wait;m.reason=state;return true};
  if(now-j.at>=introMockPolicy.deadline)return fail('timeout',introMockPolicy.cooldown);
  if(j.fail&&j.scenario==='timeout')return false;
  if(now-j.at<introMockPolicy.delay)return false;
  if(j.fail)return fail(j.scenario==='rate-limit'?'rate_limited':'http_error',j.scenario==='rate-limit'?introMockPolicy.rateCooldown:introMockPolicy.cooldown);
  m.state=j.topic===1&&!j.s.album.trim()?'ambiguous':j.scenario==='unavailable'?'insufficient_evidence':'ready';
  m.value=m.state==='ready'?{subject:j.s[['artist','album','title'][j.topic]],paragraphs:[...introMockCatalog.paragraphs[j.topic]],source:'Local sample (synthetic)',sources:[{title:'Local sample (synthetic)',url:'https://example.org/local-introduction'}],paragraphSources:[[0],[0]]}:null;
  m.job=null;m.cache=m.cache.filter(e=>e.key!==j.key); // fixed three topics for one track
  m.cache.push({key:j.key,state:m.state,value:m.value,at:now,used:now});return true;
 };
 return m;
}
