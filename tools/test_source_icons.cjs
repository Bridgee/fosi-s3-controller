const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
for(const name of ['index.html','orbit-s3-preview.html']){
 const source=fs.readFileSync(path.join(root,name),'utf8');
 const registry=source.match(/\/\/ orbit-source-registry:start([\s\S]*?)\/\/ orbit-source-registry:end/)[1];
 const identify=new Function(registry+'\nreturn networkSource;')();
 const cases=[['AirPlay','AirPlay','airplay'],[' SPOTIFY_CONNECT ','Spotify Connect','spotify'],
  ['Google-Cast','Google Cast','cast'],['tidalconnect','Tidal Connect','tidal'],
  ['qobuzconnect','Qobuz Connect','network'],['roon','Roon','roon'],
  ['UPnP','UPnP','upnp'],['DLNA','DLNA','dlna']];
 for(const [id,label,icon]of cases)assert.deepEqual(identify(id),{label,icon});
 for(const id of ['',null,'future-protocol','constructor','toString','__proto__'])
  assert.deepEqual(identify(id),{label:'Network',icon:'network'});
 for(const [attribute,key] of [['data-input="line"','line'],['data-input="hdmi"','hdmi'],['data-input="optical"','optical'],['data-output="optical"','optical'],['data-output="analog"','analog']]) {
  const choice=source.match(new RegExp('<button[^>]*'+attribute+'[^>]*>([\\s\\S]*?)</button>'));
  assert(choice && choice[1].includes('href="#orbit-src-'+key+'"'),name+' physical connector asset mismatch');
 }
 const current=new Function('state','shortLabels',registry+'\nreturn currentSource();');
 assert.deepEqual(current({input:'unknown'},{}),{label:'Unknown source',icon:null});
 assert.deepEqual(current({input:'network',networkActive:false},{}),{label:'Network',icon:'network'});
 const ids=[...source.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]);
 assert.equal(ids.length,new Set(ids).size,name+' duplicate IDs');
 for(const match of source.matchAll(/<svg([^>]*)><use[^>]*href="([^"]+)"/g)){
  if(match[2]==='#previewAlbum' || match[2]==='#orbitPreviewAlbum'){
   assert.match(match[1],/viewBox="0 0 400 400"/);continue;
  }
  assert.match(match[1],/viewBox="0 0 24 24"/);
  assert.match(match[1],/preserveAspectRatio="xMidYMid meet"/);
 }
}
process.stdout.write('PASS: source identities/aliases, unknown IDs and both previews\' SVG aspect ratios\n');
