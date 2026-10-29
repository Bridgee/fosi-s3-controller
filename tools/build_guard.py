"""Never upload verification or local sample-data environments."""
Import("env")
from SCons.Script import COMMAND_LINE_TARGETS
from pathlib import Path
import hashlib

root=Path(env["PROJECT_DIR"])
driver=root/".deps/LilyGo-T-RGB/src/LilyGo_RGBPanel.cpp"
expected=(root/"patches/t-rgb-driver.sha256").read_text().strip()
if not driver.exists() or hashlib.sha256(driver.read_bytes()).hexdigest()!=expected:
    raise RuntimeError("Run python3 tools/prepare.py: the reviewed T-RGB safety patch is missing or differs")

if env["PIOENV"] in ("orbit-live-check", "orbit-intro-mock", "orbit-ai-check") and any(
        target in COMMAND_LINE_TARGETS for target in ("upload", "uploadfs", "uploadfsota")):
    raise RuntimeError("Verification/sample-data targets cannot be uploaded; use orbit-demo or configured orbit-live")

# A UI/version change must update the two local previews before any build.
# Read-only: does not load private headers, initialize a provider or open devices.
import importlib.util
spec=importlib.util.spec_from_file_location("orbit_preview_sync",root/"tools/sync_preview.py")
preview_sync=importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview_sync)
try:preview_sync.check()
except (AssertionError,OSError,ValueError,KeyError) as error:
    raise RuntimeError("Local preview is stale; run python3 tools/sync_preview.py: "+str(error)) from None
