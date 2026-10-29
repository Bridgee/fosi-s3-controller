"""Run the actual discovery selection policy under ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="orbit-discovery-") as temp:
    binary=str(Path(temp)/"discovery-test")
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-g","-fsanitize=address,undefined",
                    "-I"+str(root/"firmware/OrbitS3"),str(root/"tests/discovery_test.cpp"),"-o",binary],check=True)
    subprocess.run([binary],check=True)

    source=(root/"firmware/OrbitS3/s3_client.cpp").read_text()
    actual=source[source.index("uint32_t ipValue("):source.index("void clearTarget(")]
    fixture=(root/"tests/discovery_worker_test.cpp").read_text().replace("// ACTUAL_DISCOVERY",actual)
    test=Path(temp)/"worker.cpp";test.write_text(fixture)
    binary=str(Path(temp)/"worker-test")
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-g","-fsanitize=address,undefined",
                    "-I"+str(root/"firmware/OrbitS3"),"-I"+str(root/".pio/libdeps/orbit-live-check/ArduinoJson/src"),
                    str(test),"-o",binary],check=True)
    subprocess.run([binary],check=True)
