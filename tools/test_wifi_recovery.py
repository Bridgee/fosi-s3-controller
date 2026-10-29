from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="wifi-recovery-") as d:
    out=str(Path(d)/"test")
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-I"+str(r/"firmware/OrbitS3"),str(r/"tests/wifi_recovery_test.cpp"),"-o",out],check=True)
    subprocess.run([out],check=True)
