"""Local loopback requests to a temporary dummy tree; no provider/device calls."""
from pathlib import Path
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer
import importlib.util,tempfile,threading
p=Path(__file__).with_name('serve_preview.py');spec=importlib.util.spec_from_file_location('preview',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='fosi-preview-') as d:
    root=Path(d);(root/'assets').mkdir();(root/'firmware/OrbitS3').mkdir(parents=True)
    (root/'index.html').write_text('<h1>Local sample</h1>');(root/'orbit-s3-preview.html').write_text('preview')
    (root/'assets/icon.svg').write_text('<svg/>');(root/'firmware/OrbitS3/ai_local.h').write_text('SYNTHETIC_PRIVATE_MARKER')
    (root/'assets/private.svg').symlink_to(root/'firmware/OrbitS3/ai_local.h')
    server=ThreadingHTTPServer(('127.0.0.1',0),m.handler(root));thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start();port=server.server_address[1]
    def request(path,host=None,method='GET'):
        c=HTTPConnection('127.0.0.1',port,timeout=2);c.request(method,path,headers={'Host':host or f'127.0.0.1:{port}'});r=c.getresponse();body=r.read();headers=dict(r.getheaders());status=r.status;c.close();assert b'SYNTHETIC_PRIVATE_MARKER' not in body;return status,headers,body
    try:
        for path in ['/','/index.html?device=intro','/orbit-s3-preview.html','/assets/icon.svg']:
            code,headers,body=request(path);assert code==200 and headers['X-Content-Type-Options']=='nosniff' and "connect-src 'none'" in headers['Content-Security-Policy']
            assert request(path,method='HEAD')[0]==200
        for path in ['/firmware/OrbitS3/ai_local.h','/firmware/OrbitS3/config_local.h','/ai/','/tools/serve_preview.py','/.git/config','/assets/','/assets/../firmware/OrbitS3/ai_local.h','/assets/%2e%2e/firmware/OrbitS3/ai_local.h','/assets/private.svg','/index.html%00','/assets%5c..%5cfirmware/OrbitS3/ai_local.h']:
            assert request(path)[0]==404,path
        assert request('/index.html',f'evil.example:{port}')[0]==403
        assert request('http://evil.example/index.html')[0]==404
    finally:server.shutdown();server.server_close();thread.join()
print('PASS: loopback preview serves only pages/assets; private/source files, listing, traversal, symlinks and DNS-rebinding Host rejected; CSP forbids network connections; dummy files only.')
