"""Negative boundary tests use disposable synthetic files, never actual secrets."""
from pathlib import Path
import contextlib
import io
import json
import subprocess
import tempfile
from check_repository import audit

with tempfile.TemporaryDirectory(prefix="fosi-public-boundary-") as d:
    root = Path(d)
    (root / "README.md").write_text("# Public\n")
    (root / "repository-files.json").write_text(json.dumps({"files": ["README.md"]}))
    audit(root)
    def rejected():
        with contextlib.redirect_stderr(io.StringIO()):
            try: audit(root)
            except ValueError: return
        raise AssertionError("Expected boundary rejection")
    (root / "context.md").write_text("Private notes")
    rejected(); (root / "context.md").unlink()
    for text in ["sk-" + "X" * 32, "https://api.day.app/" + "X" * 24,
                 "#define ORBIT_WIFI_PASSWORD \"synthetic-unapproved-password\"",
                 "[bad](../outside.md)", "non-English: \u4e2d\u6587"]:
        (root / "README.md").write_text(text); rejected()
    (root / "README.md").write_text("# Public\n")
    outside = root.parent / (root.name + "-outside")
    outside.write_text("synthetic")
    (root / "README.md").unlink(); (root / "README.md").symlink_to(outside)
    rejected(); (root / "README.md").unlink(); outside.unlink()
    (root / "README.md").write_text("# Public\n")
    subprocess.run(["git", "init", "-q", str(root)], check=True)
    (root / "config_local.h").write_text("// synthetic private fixture")
    subprocess.run(["git", "-C", str(root), "add", "config_local.h"], check=True)
    rejected()
print("PASS: injected context, tokens, private settings, links, non-English prose, symlinks and tracked private file rejected")
