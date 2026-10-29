"""Build a static, API-free simulator artifact; never publish the repository root."""
from pathlib import Path
from html import escape
import argparse
import hashlib
import json
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PAGES = ("index.html", "orbit-s3-preview.html")
CSP = ("default-src 'none'; script-src 'self' 'unsafe-inline'; "
       "style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; "
       "connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; "
       "worker-src 'none'; media-src 'none'")
EXTRA = {"assets/clock/pixel-glyphs.js", "assets/timezones/rules.js",
         "assets/fonts/OFL-Montserrat.txt", "assets/fonts/OFL-NotoSansCJK.txt",
         "assets/fonts/UNICODE-DATA-LICENSE.txt", "assets/source-icons/CC0-1.0.txt",
         "LICENSE", "THIRD_PARTY_NOTICES.md"}
HOME_LINK = '<a class="site-home" href="https://bridgee.github.io/" aria-label="Back to Bridge Zhao home"><span aria-hidden="true">&#8592;</span> Bridge Zhao</a>'
HOME_STYLE = """<style id="site-navigation-style">
.site-home{position:fixed;top:max(16px,env(safe-area-inset-top));left:max(16px,env(safe-area-inset-left));z-index:40;display:inline-flex;align-items:center;gap:9px;padding:11px 15px;min-height:44px;border:1px solid #cbd1c8;border-radius:24px;background:#eeefeaed;color:#344b40;font:500 13px/20px -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;text-decoration:none;backdrop-filter:blur(8px)}
.site-home:hover{background:#fff;color:#193b2a;border-color:#8ea395}
.site-home:focus-visible{outline:2px solid #3d6252;outline-offset:4px}
@media(max-height:600px){.site-home{top:8px;left:8px;padding:8px 12px}}
</style>"""
WEB_SUFFIXES = {".js", ".css", ".svg", ".png", ".ttf", ".woff", ".woff2", ".txt"}
NETWORK_OR_DEVICE = re.compile(
    r"\b(?:fetch|XMLHttpRequest|WebSocket|EventSource|Worker|SharedWorker|importScripts)\s*\("
    r"|\bsendBeacon\s*\(|\bnavigator\s*\.\s*(?:serial|usb|bluetooth|geolocation|serviceWorker)\b"
)


def selected(name):
    if name in PAGES or name in EXTRA: return True
    return (name.startswith(("assets/preview/", "assets/weather/"))
            and Path(name).suffix in WEB_SUFFIXES)


def build(destination, root=ROOT):
    root = Path(root).resolve(); destination = Path(destination)
    if destination.exists() or destination.is_symlink():
        raise ValueError("Output already exists; choose a fresh directory")
    if root == destination.resolve() or destination.resolve() in root.parents:
        raise ValueError("Output cannot replace repository or an ancestor")
    approved = json.loads((root / "repository-files.json").read_text())["files"]
    names = sorted(name for name in approved if selected(name))
    if not set(PAGES + tuple(EXTRA)).issubset(names):
        raise ValueError("Required static pages/licenses missing from public inventory")
    payload = {}
    for name in names:
        source = root / name
        if Path(name).is_absolute() or ".." in Path(name).parts:
            raise ValueError("Unsafe static inventory path")
        if source.is_symlink() or not source.resolve().is_relative_to(root):
            raise ValueError("Static input crosses repository boundary: " + name)
        if any(p.is_symlink() for p in source.parents if p != root and root in p.parents):
            raise ValueError("Static input has a symlink parent: " + name)
        data = source.read_bytes()
        if source.suffix in {".html", ".js", ".css", ".svg"}:
            text = data.decode("utf-8")
            if source.suffix in {".html", ".js"} and NETWORK_OR_DEVICE.search(text):
                raise ValueError("Network/device API present in simulator: " + name)
            # Resource attributes must remain relative or inline. Source citations are links,
            # not automatically loaded resources, and remain usable by an explicit click.
            if re.search(r'(?:src|action)\s*=\s*["\'](?:https?:|//|/)', text, re.I):
                raise ValueError("Non-relative resource in simulator: " + name)
            if name in PAGES:
                policy = '<meta http-equiv="Content-Security-Policy" content="' + escape(CSP, quote=True) + '">'
                title = '<title>Fosi S3 Controller — Online simulator</title>'
                if re.search(r'<head>\s*<meta charset="UTF-8">', text):
                    text = text.replace('<meta charset="UTF-8">', '<meta charset="UTF-8">\n' + policy
                                        + '\n<meta name="referrer" content="no-referrer">', 1)
                    text = re.sub(r'<title>.*?</title>', title, text, count=1)
                elif text.startswith('<meta charset="utf-8">'):
                    # The alternate entry is an embeddable fragment. Give only the hosted
                    # artifact an explicit head so its policy precedes all resources.
                    body = re.sub(r'^<meta charset="utf-8">\s*<title>.*?</title>\s*', '', text, count=1)
                    text = ('<!doctype html>\n<html lang="en"><head><meta charset="UTF-8">\n' + policy
                            + '\n<meta name="referrer" content="no-referrer">'
                            + '<meta name="viewport" content="width=device-width,initial-scale=1">'
                            + title + '</head><body>' + body + '</body></html>')
                else:
                    raise ValueError("Unexpected page head; CSP must precede resources")
                # Hosting navigation lives outside the simulated display. A regular link
                # leaves the simulation only on an explicit click; connect-src stays none.
                text = text.replace('</head>', HOME_STYLE + '</head>', 1)
                text = text.replace('<body>', '<body>' + HOME_LINK, 1)
                data = text.encode("utf-8")
        payload[name] = data
    manifest = {"mode":"offline_simulation", "networkApis":False, "deviceAccess":False,
                "files":{name:hashlib.sha256(data).hexdigest() for name,data in payload.items()}}
    payload["site-manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fosi-pages-", dir=destination.parent) as scratch:
        staged = Path(scratch) / "site"; staged.mkdir()
        for name, data in payload.items():
            out = staged / name; out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(data)
        staged.rename(destination)
    print(f"PASS: static simulator artifact: {len(payload)} files, {sum(map(len,payload.values())):,} bytes; no firmware/config/tests")
    return set(payload)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "_site")
    build(parser.parse_args().output)
