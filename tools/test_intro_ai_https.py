"""Compile and execute the real HTTP/config/transport with fake TLS only."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='fosi-ai-https-') as d:
    binary=Path(d)/'https-test'
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g',
                    '-DORBIT_BUILD_CHECK=1','-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3'),
                    *[str(r/'firmware/OrbitS3'/n) for n in ['intro_ai.cpp','intro_ai_settings.cpp','intro_ai_http.cpp','intro_ai_https.cpp']],
                    str(r/'tests/intro_ai_https_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(r/'tests/fixtures/intro_ai_response.json')],check=True)

with tempfile.TemporaryDirectory(prefix='fosi-ai-esp32-fake-') as d:
    binary=Path(d)/'backend-test'
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g',
                    '-DORBIT_BUILD_CHECK=1','-DORBIT_AI_HOST_TEST=1','-I'+str(r/'tests/fakes'),'-I'+str(r/'firmware/OrbitS3'),
                    *[str(r/'firmware/OrbitS3'/n) for n in ['intro_ai_settings.cpp','intro_ai_http.cpp','intro_ai_https.cpp','intro_ai_esp32.cpp']],
                    str(r/'tests/intro_ai_esp32_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(r/'tests/fixtures/intro_ai_response.json')],check=True)

# Compile the ordinary disabled factory without any fake/SDK headers. It cannot
# even link a socket or TLS implementation. Also verify a check build cannot
# enable settings even when an enable macro is deliberately supplied.
with tempfile.TemporaryDirectory(prefix='fosi-ai-off-') as d:
    w=Path(d);(w/'ai_local.h').write_text('#error "PRIVATE AI FILE MUST NOT BE READ"\n')
    (w/'off.cpp').write_text('#include "intro_ai_esp32.h"\n#include <cassert>\nint main(){assert(!orbit_intro_ai::configuredApiSettings().enabled);assert(orbit_intro_ai::createEsp32Channel()==nullptr);}\n')
    binary=w/'off'
    subprocess.run(['c++','-std=c++11','-DORBIT_BUILD_CHECK=1','-I'+str(w),'-I'+str(r/'firmware/OrbitS3'),str(w/'off.cpp'),str(r/'firmware/OrbitS3/intro_ai_settings.cpp'),str(r/'firmware/OrbitS3/intro_ai_esp32.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    (w/'check.cpp').write_text('#include "intro_ai_settings.h"\n#include <cassert>\nint main(){assert(!orbit_intro_ai::configuredApiSettings().enabled);}\n')
    subprocess.run(['c++','-std=c++11','-DORBIT_AI_ENABLED=1','-DORBIT_BUILD_CHECK=1','-I'+str(w),'-I'+str(r/'firmware/OrbitS3'),str(w/'check.cpp'),str(r/'firmware/OrbitS3/intro_ai_settings.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: disabled factory needs no SDK/network symbols; compile-only settings stay off and ignore private credential headers.')
