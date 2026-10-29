"""Test the upload guard without invoking an uploader or touching a device."""
from pathlib import Path
import runpy
import sys
import types

script=types.ModuleType("SCons.Script")
sys.modules["SCons"]=types.ModuleType("SCons")
sys.modules["SCons.Script"]=script
path=Path(__file__).with_name("build_guard.py")
for environment,targets,blocked in [
    ("orbit-demo",["upload"],False),("orbit-live",["upload"],False),
    ("orbit-live-check",[],False),("orbit-live-check",["upload"],True),
    ("orbit-live-check",["uploadfs"],True),("orbit-live-check",["uploadfsota"],True),
    ("orbit-ai-check",[],False),("orbit-ai-check",["upload"],True),
    ("orbit-ai-check",["uploadfs"],True),("orbit-ai-check",["uploadfsota"],True),
    ("orbit-intro-mock",[],False),("orbit-intro-mock",["upload"],True),
    ("orbit-intro-mock",["uploadfs"],True),("orbit-intro-mock",["uploadfsota"],True)]:
    script.COMMAND_LINE_TARGETS=targets
    failed=False
    try:runpy.run_path(str(path),init_globals={"env":{"PIOENV":environment,"PROJECT_DIR":str(path.resolve().parents[1])},"Import":lambda _:None})
    except RuntimeError:failed=True
    assert failed==blocked,(environment,targets)
print("PASS: verification-target upload guard; no uploader/device invoked")
