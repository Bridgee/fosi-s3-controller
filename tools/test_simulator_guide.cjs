// Guide coverage, overlay precedence and common format; no browser or network.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const {guides,selectGuide}=require('../assets/preview/guide.js');
let count=0;
for(const name of ['index.html','orbit-s3-preview.html']){
  const text=fs.readFileSync(path.join(root,name),'utf8');
  for(const [,view] of text.matchAll(/id="page-([^"]+)"/g)){
    assert(Object.hasOwn(guides,view),'Missing guide: '+view);
    assert.equal(selectGuide({view}).key,view);count++;
  }
  assert.equal(text.split('src="assets/preview/guide.js"').length,2);
  assert.equal(text.split('href="assets/preview/guide.css"').length,2);count+=2;
}
for(const [key,guide] of Object.entries(guides)){
  assert(guide.title && guide.summary && guide.navigation,key);
  assert(guide.actions.length>0&&guide.actions.length<=4,key);
  assert(guide.actions.every(value=>typeof value==='string'&&value.length<180));
  assert(guide.navigation.length<210);count+=4;
}
assert.equal(selectGuide({view:'player',asleep:true,standby:true,artwork:true}).key,'asleep');
assert.equal(selectGuide({view:'player',standby:true,artwork:true}).key,'standby');
assert.equal(selectGuide({view:'player',artwork:true}).key,'artwork');
assert.equal(selectGuide({view:'player',vinyl:true}).key,'vinyl');
assert.equal(selectGuide({view:'blank-up',standby:true,vinyl:true}).key,'blank-up');
assert.equal(selectGuide({view:'lyrics',artwork:true}).key,'lyrics');
assert.equal(selectGuide({view:'__proto__'}).key,'player');
assert.equal(selectGuide({view:'unrecognized'}).key,'player');
assert.equal(selectGuide({view:'intro-read'}).key,'intro-read');count+=9;
assert(guides['blank-up'].navigation.includes('Swipe up'));
assert(guides['blank-down'].navigation.includes('Swipe down'));
assert(guides['blank-left'].navigation.includes('Swipe left'));
assert(guides['blank-right'].navigation.includes('Swipe right'));count+=4;
console.log(`PASS: ${count} simulator guide checks: every page, consistent format, navigation and asleep/standby/artwork/vinyl precedence; no APIs or devices.`);
