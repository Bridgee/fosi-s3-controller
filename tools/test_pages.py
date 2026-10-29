"""Static Pages safety and project-subpath tests; synthetic loopback traffic only."""
from pathlib import Path
from html.parser import HTMLParser
from http.client import HTTPConnection
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urljoin, urlsplit, unquote
import hashlib
import json
import shutil
import tempfile
import threading
from build_pages import build, ROOT, CSP, PAGES, selected, HOME_LINK

class Resources(HTMLParser):
    def __init__(self): super().__init__(); self.paths=[]; self.policies=[]
    def handle_starttag(self, tag, attrs):
        a=dict(attrs)
        if 'src' in a:self.paths.append(a['src'])
        if tag=='meta' and a.get('http-equiv')=='Content-Security-Policy':self.policies.append(a['content'])

with tempfile.TemporaryDirectory(prefix='fosi-pages-test-') as d:
    root=Path(d);out=root/'fosi-s3-controller';names=build(out)
    assert all(not name.startswith(('firmware/','tools/','tests/','ai/','.git')) for name in names)
    assert not any(name.endswith(('_local.h','.bin','.py','.cpp','.ino')) for name in names)
    manifest=json.loads((out/'site-manifest.json').read_text())
    assert manifest['mode']=='offline_simulation' and manifest['networkApis'] is False and manifest['deviceAccess'] is False
    for name,sha in manifest['files'].items():assert hashlib.sha256((out/name).read_bytes()).hexdigest()==sha
    for page in PAGES:
        text=(out/page).read_text();parser=Resources();parser.feed(text)
        assert text.count(HOME_LINK)==1 and 'id="site-navigation-style"' in text
        assert text.index(HOME_LINK)<text.index('id="screen"')
        assert 'href="https://bridgee.github.io/"' in HOME_LINK
        assert parser.policies==[CSP] and "connect-src 'none'" in CSP
        assert "form-action 'none'" in CSP and "worker-src 'none'" in CSP and "base-uri 'none'" in CSP
        assert text.index('Content-Security-Policy')<text.index('<style>')<text.index('<script')
        for name in parser.paths:
            url=urljoin('https://example.invalid/fosi-s3-controller/'+page,name)
            assert url.startswith('https://example.invalid/fosi-s3-controller/')
            assert (root/unquote(urlsplit(url).path.lstrip('/'))).is_file(),name
    try:build(out)
    except ValueError:pass
    else:raise AssertionError('Existing output must not be replaced')
    # Unknown/private content never becomes a site asset even if it exists in the checkout.
    for name in ['firmware/OrbitS3/config_local.h','references/notes.md','assets/preview/config_local.h','.git/config','README.md','assets/preview/manifest.json']:
        assert not selected(name),name
    class Server(SimpleHTTPRequestHandler):
        def __init__(self,*a,**kw):super().__init__(*a,directory=str(root),**kw)
        def log_message(self,*a):pass
    server=ThreadingHTTPServer(('127.0.0.1',0),Server);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    def get(path):
        c=HTTPConnection('127.0.0.1',server.server_address[1],timeout=2);c.request('GET',path);r=c.getresponse();data=r.read();code=r.status;c.close();return code,data
    try:
        assert get('/fosi-s3-controller/')[0]==200
        for page in PAGES:assert get('/fosi-s3-controller/'+page+'?device=player')[0]==200
        for name in names:assert get('/fosi-s3-controller/'+name)[0]==200,name
        for name in ['firmware/OrbitS3/config_local.h','firmware/OrbitS3/OrbitS3.ino','tests/ui_smoke_test.cpp','ai/config.example.json','.git/config']:
            assert get('/fosi-s3-controller/'+name)[0]==404,name
    finally:server.shutdown();server.server_close();thread.join()
    # Reject unsafe active code and symlinked resources before creating a deployable artifact.
    fixture=root/'fixture';fixture.mkdir();(fixture/'assets/preview').mkdir(parents=True)
    shutil.copyfile(ROOT/'repository-files.json',fixture/'repository-files.json')
    approved=json.loads((fixture/'repository-files.json').read_text())['files']
    for name in approved:
        if selected(name):
            p=fixture/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,p)
    original=(fixture/'index.html').read_text()
    (fixture/'index.html').write_text(original.replace('</head>', '<script>fetch("/trap")</script></head>'))
    try:build(root/'bad-network',fixture)
    except ValueError:assert not (root/'bad-network').exists()
    else:raise AssertionError('Network-enabled input accepted')
    (fixture/'index.html').write_text(original)
    asset=fixture/'assets/clock/pixel-glyphs.js';asset.unlink();asset.symlink_to(ROOT/'assets/clock/pixel-glyphs.js')
    try:build(root/'bad-link',fixture)
    except ValueError:assert not (root/'bad-link').exists()
    else:raise AssertionError('Symlink input accepted')
print('PASS: static inventory/headers/hash/resource paths, project-subpath serving, private/source404, no overwrite, network-code and symlink rejection; no real API/device requests')
