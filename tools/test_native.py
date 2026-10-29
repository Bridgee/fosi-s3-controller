"""Run the maintained offline firmware regressions; no API or device access."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TESTS = (
    "test_repository.py", "test_protocol.py", "test_discovery.py", "test_source_status.py",
    "test_bounded_body.py", "test_http_headers.py", "test_http_lifetime.py",
    "test_json_security.py", "test_wifi_recovery.py", "test_location_recovery.py",
    "test_playback_progress.py", "test_appearance.py", "test_time_tools.py",
    "test_screen_idle.py", "test_status_feedback.py", "test_weather.py",
    "test_lyrics.py", "test_iphone_trigger.py", "test_artwork_retry.py",
    "test_intro_ai.py", "test_intro_ai_https.py", "test_intro_ai_async.py",
    "test_build_guard.py",
)
if __name__ == "__main__":
    failures = []
    for name in TESTS:
        print("Running " + name, flush=True)
        try:
            result = subprocess.run([sys.executable, str(ROOT / "tools" / name)], cwd=ROOT, timeout=180)
            if result.returncode: failures.append(name)
        except subprocess.TimeoutExpired:
            print("FAIL: suite timed out: " + name, flush=True)
            failures.append(name)
    if failures:
        print("FAIL: " + ", ".join(failures), flush=True)
        raise SystemExit(1)
    print(f"PASS: {len(TESTS)} offline regression suites")
