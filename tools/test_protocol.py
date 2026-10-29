"""Compile and run the actual firmware wire-code tests on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
library = root / ".pio/libdeps/orbit-live-check/ArduinoJson/src"
if not library.exists():
    raise SystemExit("Build orbit-live-check first to fetch its pinned ArduinoJson dependency.")
with tempfile.TemporaryDirectory(prefix="orbit-protocol-") as temp:
    binary = str(Path(temp) / "protocol-test")
    subprocess.run(["c++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-g",
                    "-I" + str(root / "firmware/OrbitS3"), "-I" + str(library),
                    str(root / "tests/protocol_test.cpp"), "-o", binary], check=True)
    subprocess.run([binary], check=True)
