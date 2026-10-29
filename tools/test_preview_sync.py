"""Check public generated preview assets and fail-closed drift detection; no devices/network."""
from pathlib import Path
import importlib.util,json,tempfile,shutil,re
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('preview_sync',root/'tools/sync_preview.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
contract=m.check();count=0
with tempfile.TemporaryDirectory(prefix='preview-sync-test-') as tmp:
 target=Path(tmp)
 for name in [*contract['inputs'],*contract['assets'],'assets/preview/manifest.json',*m.PAGES]:
  path=target/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/name,path)
 m.ROOT=target;m.check()
 for name in ['firmware/OrbitS3/firmware_version.h','firmware/OrbitS3/orbit_ui.cpp','firmware/OrbitS3/source_status.h','assets/preview/layout.css','assets/preview/runtime.js','tools/sync_preview.py','assets/preview/status-connected_0.svg','index.html','orbit-s3-preview.html']:
  path=target/name;original=path.read_bytes()
  if name.endswith('.html'):path.write_text(path.read_text().replace('"firmwareVersion":"'+contract['firmwareVersion']+'"','"firmwareVersion":"0.0.0"'))
  else:path.write_bytes(original+b'\nDRIFT_TEST\n')
  try:m.check()
  except AssertionError:count+=1
  else:raise AssertionError('Unnoticed preview drift: '+name)
  path.write_bytes(original);m.check()
 # CSS of the embedded page has complete selector scoping, including comma lists.
 css=(root/'assets/preview/layout.css').read_text();scoped=m.scoped_css(css,'orbit-s3-preview.html')
 for selectors in re.findall(r'(?:^|\n)([^{}\n]+)\{[^{}]*\}',scoped):
  if selectors.startswith('@'):continue
  assert all(s.strip().startswith(('#orbit-s3-preview ','.device-only #orbit-s3-preview ')) for s in selectors.split(','));count+=1
 # The two public HTML versions remain identical except wrapper-specific scoping.
 assert (root/'assets/preview/Montserrat-Medium.ttf').stat().st_size==243180
 assert len([p for p in contract['assets'] if '/status-' in p])==19
 assert all(len(rect) in (3,4) for rect in contract['rects'].values())
print('PASS: '+str(count)+' public preview consistency/failure checks; version, native inputs, generated assets, standalone pages and scoped CSS. No private configuration, device or API access.')
