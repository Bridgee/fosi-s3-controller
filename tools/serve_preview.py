"""Loopback-only, allowlisted static preview; never serve project/config sources."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlsplit, unquote
import argparse

ROOT=Path(__file__).resolve().parents[1]
PUBLIC={"/index.html","/orbit-s3-preview.html"}
EXTENSIONS={".svg",".png",".jpg",".jpeg",".webp",".gif",".json",".html",".woff",".woff2",".ttf",".otf",".txt",".css",".js"}

def handler(root=ROOT):
    root=root.resolve()
    class Preview(SimpleHTTPRequestHandler):
        def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(root),**kwargs)
        def log_message(self,*args):pass
        def list_directory(self,path):self.send_error(404);return None
        def end_headers(self):
            self.send_header("Cache-Control","no-store")
            self.send_header("X-Content-Type-Options","nosniff")
            self.send_header("Content-Security-Policy","default-src 'self' data:; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'none'; object-src 'none'; frame-ancestors 'none'")
            super().end_headers()
        def send_head(self):
            hosts=self.headers.get_all("Host") or []
            port=self.server.server_address[1]
            if len(hosts)!=1 or hosts[0] not in {f"127.0.0.1:{port}",f"localhost:{port}"}:
                self.send_error(403);return None
            try:
                split=urlsplit(self.path)
                if split.scheme or split.netloc:raise ValueError()
                path=unquote(split.path,errors="strict")
                if not path.startswith("/") or "\\" in path or any(ord(c)<32 for c in path):raise ValueError()
                if any(p in {".",".."} or p.startswith(".") for p in path.split("/") if p):raise ValueError()
                if path=="/":path="/index.html"
                file=(root/path.lstrip("/")).resolve()
                allowed=path in PUBLIC and file.parent==root
                if path.startswith("/assets/") and (root/"assets") in file.parents and file.suffix.lower() in EXTENSIONS:allowed=True
                if not allowed or not file.is_file():self.send_error(404);return None
            except (ValueError,UnicodeError,OSError):
                self.send_error(404);return None
            # Resolve once, then open only the already-authorized file. No
            # directory listing, source tree fallback or decoded-path re-routing.
            try:stream=file.open("rb")
            except OSError:self.send_error(404);return None
            self.send_response(200);self.send_header("Content-Type",self.guess_type(str(file)))
            self.send_header("Content-Length",str(file.stat().st_size));self.end_headers();return stream
    return Preview

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--port",type=int,default=8765);args=parser.parse_args()
    if not 1<=args.port<=65535:parser.error("port must be1..65535")
    server=ThreadingHTTPServer(("127.0.0.1",args.port),handler())
    print(f"Local preview: http://127.0.0.1:{args.port}/index.html",flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
