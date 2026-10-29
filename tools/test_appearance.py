"""Exercise production appearance record handling using a fake NVS Preferences transport."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="orbit-appearance-") as d:
 binary=Path(d)/"appearance-test"
 subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-I"+str(r/"tests/fakes"),"-I"+str(r/"firmware/OrbitS3"),str(r/"firmware/OrbitS3/appearance_settings.cpp"),str(r/"tests/appearance_settings_test.cpp"),"-o",str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
