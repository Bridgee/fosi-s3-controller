"""Production weather/location worker under network-generation and failure races; no I/O."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='orbit-location-recovery-') as d:
 w=Path(d)
 for name in ['Arduino.h','WiFi.h','WiFiClientSecure.h','HTTPClient.h','esp_heap_caps.h','freertos/FreeRTOS.h','freertos/task.h','freertos/queue.h']:
  p=w/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "weather_platform.h"\n')
 b=w/'test';subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g','-DORBIT_LIVE=1','-DORBIT_BUILD_CHECK=1','-I'+str(w),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3'),str(r/'tests/location_recovery_test.cpp'),str(r/'firmware/OrbitS3/weather.cpp'),str(r/'firmware/OrbitS3/timezone_data.cpp'),'-o',str(b)],check=True)
 failed=False
 for case in ['stale-location','offline-response','rate-limit','stale-forecast','initial-failure','initial-flaps']:
  result=subprocess.run([str(b),str(r/'tests/fixtures/weather.json'),case]);print(case,result.returncode,flush=True);failed|=result.returncode!=0
 if failed:raise SystemExit(1)
