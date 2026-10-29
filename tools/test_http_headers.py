"""Check the actual SDK transport adapter without a network or device."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="orbit-headers-") as temp:
    binary=str(Path(temp)/"headers-test")
    subprocess.run(["c++","-std=c++14","-Wall","-Wextra","-Werror",
                    "-fsanitize=address,undefined","-g",
                    "-I"+str(root/"firmware/OrbitS3"),
                    str(root/"tests/http_header_budget_test.cpp"),"-o",binary],check=True)
    subprocess.run([binary],check=True)
