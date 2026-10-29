from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="status-feedback-") as d:
    binary=str(Path(d)/"test")
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-I"+str(root/"firmware/OrbitS3"),str(root/"tests/status_feedback_test.cpp"),"-o",binary],check=True)
    subprocess.run([binary],check=True)
