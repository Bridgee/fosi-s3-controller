"""Check public repository boundaries without printing matched secret contents."""
from pathlib import Path
import json
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "repository-files.json"
IGNORED_DIRS = {".git", ".deps", ".pio", ".pio-core", ".venv", "__pycache__", ".vscode", ".idea", "artifacts", "_site"}
PATTERNS = {
    "provider token": re.compile(r"\b(?:sk-[A-Za-z0-9_-]{16,}|gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})"),
    "private key": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    "push key URL": re.compile(r"https://api\.day\.app/(?!push(?:[\s\"/]|$))[A-Za-z0-9_-]{8,}"),
    "private workstation path": re.compile(r"(?:/" + r"Users/[^/\s\"']+/|/" + r"home/[^/\s\"']+/|[A-Z]:\\Users\\[^\\]+\\)"),
}
SECRET_DEFINE = re.compile(r'^\s*#define\s+ORBIT_(?:WIFI_PASSWORD|WIFI_SSID|BARK_DEVICE_KEY|AI_API_KEY|S3_URL)\s+"([^"\n]+)"', re.M)
EXAMPLES = {"your-2.4GHz-wifi", "your-wifi-password", "HOST_TEST_DEVICE_KEY", "BUILD_CHECK_ONLY"}


def audit(root=ROOT):
    root = Path(root)
    manifest = json.loads((root / "repository-files.json").read_text())
    files = manifest["files"]
    if files != sorted(set(files)):
        raise ValueError("Public file manifest must be unique and sorted")
    allowed = set(files) | {"repository-files.json"}
    problems = []
    for name in sorted(allowed):
        p = Path(name)
        if p.is_absolute() or ".." in p.parts or any(c in IGNORED_DIRS for c in p.parts):
            problems.append((name, "unsafe manifest path")); continue
        path = root / name
        if path.is_symlink() or any(parent.is_symlink() for parent in path.parents if parent != root and root in parent.parents):
            problems.append((name, "symlink outside public boundary")); continue
        if not path.is_file():
            problems.append((name, "missing public file")); continue
        text = path.read_bytes().decode("utf-8", errors="ignore")
        for reason, pattern in PATTERNS.items():
            if pattern.search(text): problems.append((name, reason))
        for value in SECRET_DEFINE.findall(text):
            if value not in EXAMPLES: problems.append((name, "non-placeholder private setting"))
        if path.suffix.lower() == ".md":
            # English prose is required; code/data may contain multilingual metadata.
            if re.search(r"[\u3400-\u9fff\u3040-\u30ff]", text):
                problems.append((name, "non-English documentation"))
            for target in re.findall(r"(?<!!)\[[^\]]+\]\(([^)]+)\)", text):
                target = target.split("#")[0]
                if not target or urlsplit(target).scheme: continue
                dest = (path.parent / unquote(target)).resolve()
                if not dest.is_relative_to(root.resolve()) or not dest.exists():
                    problems.append((name, "broken or escaping local document link"))
    actual = set()
    def visit(folder):
        for p in folder.iterdir():
            if p.is_dir() and p.name in IGNORED_DIRS: continue
            name = p.relative_to(root).as_posix()
            if p.name.endswith(("_local.h", ".ino.cpp")) or p.name.startswith(".env") or p.suffix in {".pyc", ".bin", ".elf", ".map", ".o", ".log", ".zip"} or p.name == ".DS_Store": continue
            if p.is_symlink(): problems.append((name, "unmanaged symlink"))
            elif p.is_dir(): visit(p)
            else: actual.add(name)
    visit(root)
    for name in sorted(actual - allowed): problems.append((name, "file outside public manifest"))
    if (root / ".git").exists():
        result = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], capture_output=True, check=True)
        for name in result.stdout.decode().split("\0"):
            if name and name not in allowed: problems.append((name, "private or unapproved tracked file"))
    if problems:
        # Paths and categories only; never include source lines or matched values.
        for name, reason in sorted(set(problems)): print(f"FAIL: {name}: {reason}", file=sys.stderr)
        raise ValueError("Public repository check failed")
    print(f"PASS: {len(allowed)} public files; boundaries, placeholders and English documentation checked")
    return allowed


if __name__ == "__main__":
    try: audit()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(type(error).__name__ + ": repository check did not pass", file=sys.stderr)
        raise SystemExit(1)
