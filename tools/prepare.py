"""Fetch the reviewed display library and apply only the recorded safety patch."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / ".deps" / "LilyGo-T-RGB"
REV = "359c03f7cd8e9aaadd4bf0f3fbf790844fd95f98"
URL = "https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB.git"
PATCH = ROOT / "patches/t-rgb-safety.patch"

def git(*args):
    return subprocess.check_output(["git", "-C", str(DEST), *args], text=True).strip()

if DEST.exists():
    if git("rev-parse", "HEAD") != REV:
        raise SystemExit("Existing display library differs from the reviewed revision; left unchanged.")
else:
    DEST.mkdir(parents=True)
    git("init")
    git("fetch", "--depth", "1", URL, REV)
    git("checkout", "--detach", "FETCH_HEAD")

if git("status", "--porcelain", "--untracked-files=no"):
    if git("diff", "HEAD") != PATCH.read_text().strip():
        raise SystemExit("Display library has changes beyond the reviewed patch; left unchanged.")
else:
    subprocess.run(["git", "-C", str(DEST), "apply", str(PATCH)], check=True)
print("Official LilyGO T-RGB driver ready:", REV, "+ reviewed safety patch")
