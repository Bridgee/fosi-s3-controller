"""Exercise upstream numeric conversion overflow regression on the actual pinned v6."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
library=root/".pio/libdeps/orbit-live-check/ArduinoJson/src"
with tempfile.TemporaryDirectory(prefix="orbit-json-security-") as temp:
    for double in (0,1):
        binary=str(Path(temp)/f"json-test-{double}")
        subprocess.run(["c++","-std=c++11","-O0","-g","-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer",f"-DARDUINOJSON_USE_DOUBLE={double}",
                        "-I"+str(root/"firmware/OrbitS3"),"-I"+str(library),
                        str(root/"tests/json_security_test.cpp"),"-o",binary],check=True)
        subprocess.run([binary],check=True)
