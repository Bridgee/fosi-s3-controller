"""Actual queue/worker/UI lifecycle using fakes only; never opens sockets/devices."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
base=['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pthread','-fsanitize=address,undefined','-g','-DORBIT_BUILD_CHECK=1','-I'+str(r/'firmware/OrbitS3'),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src')]
with tempfile.TemporaryDirectory(prefix='fosi-intro-async-') as d:
    d=Path(d)
    def run(name,flags,modules,test):
        binary=d/name
        subprocess.run([*base,*flags,*[str(r/'firmware/OrbitS3'/m) for m in modules],str(r/'tests'/test),'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(r/"tests/fixtures/intro_ai_response.json")],check=True)
    common=['intro_ai.cpp','intro_ai_async.cpp']
    run('queues',['-DORBIT_INTRO_MOCK=1'],common+['intro_ai_mock.cpp'],'intro_ai_async_test.cpp')
    run('runtime',['-DORBIT_INTRO_MOCK=1','-DORBIT_AI_RUNTIME_HOST_TEST=1'],common+['intro_ai_mock.cpp','intro_ai_runtime.cpp'],'intro_ai_runtime_task_test.cpp')
    # A poison private header proves the runtime-check include path stays closed.
    (d/'ai_local.h').write_text('#error "private credentials must not be included"\n')
    run('runtime-off',['-DORBIT_AI_ENABLED=1','-DORBIT_AI_TRANSPORT_CHECK=1','-DORBIT_AI_RUNTIME_HOST_TEST=1','-I'+str(d)],common+['intro_ai_settings.cpp','intro_ai_http.cpp','intro_ai_https.cpp','intro_ai_runtime.cpp','intro_ai_gate.cpp'],'intro_ai_runtime_task_test.cpp')
