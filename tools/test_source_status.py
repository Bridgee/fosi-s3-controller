from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/"firmware/OrbitS3/s3_client.cpp").read_text()
start=source.index("        if(int32_t(millis()-nextPoll)>=0) {")
end=source.index("        if(ORBIT_DIAGNOSTICS",start)
actual=source[start:end]
command=source[source.index("        if(!powerInFlight.load())"):start]
send=source[source.index("bool send(Action action,int value) {"):source.index("\nbool receiveArtwork",source.index("bool send(Action action,int value) {"))]
fixture=(root/"tests/source_status_worker_test.cpp").read_text().replace("// ACTUAL_POLL",actual).replace("// ACTUAL_COMMAND",command).replace("// ACTUAL_SEND",send)
with tempfile.TemporaryDirectory(prefix="source-status-") as d:
    path=Path(d)/"test.cpp";path.write_text(fixture);binary=str(Path(d)/"test")
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror","-fsanitize=address,undefined","-g","-I"+str(root/"firmware/OrbitS3"),"-I"+str(root/".pio/libdeps/orbit-live-check/ArduinoJson/src"),str(path),"-o",binary],check=True)
    subprocess.run([binary],check=True)
