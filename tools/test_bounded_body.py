"""Exercise the actual response body reader without a network or device."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="orbit-body-") as temp:
    binary=str(Path(temp)/"body-test")
    subprocess.run(["c++","-std=c++14","-Wall","-Wextra","-Werror",
                    "-fsanitize=address,undefined","-g",
                    "-I"+str(root/"firmware/OrbitS3"),
                    str(root/"tests/bounded_body_test.cpp"),"-o",binary],check=True)
    subprocess.run([binary],check=True)
