"""Actual weather parsers and worker with fake TLS/HTTP; no location or weather requests."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='orbit-weather-') as d:
 w=Path(d)
 for name in ['Arduino.h','WiFi.h','WiFiClientSecure.h','HTTPClient.h','esp_heap_caps.h','freertos/FreeRTOS.h','freertos/task.h','freertos/queue.h']:
  p=w/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "weather_platform.h"\n')
 flags=['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g','-DORBIT_LIVE=1','-DORBIT_BUILD_CHECK=1','-I'+str(w),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3')]
 for name,sources in [('protocol',['tests/weather_protocol_test.cpp','firmware/OrbitS3/weather_labels.cpp']),('worker',['tests/weather_worker_test.cpp','firmware/OrbitS3/weather.cpp']),('auto-location',['tests/auto_location_worker_test.cpp','firmware/OrbitS3/weather.cpp'])]:
  binary=w/name;subprocess.run(flags+[str(r/p) for p in sources]+[str(r/'firmware/OrbitS3/timezone_data.cpp'),'-o',str(binary)],check=True);subprocess.run([str(binary),str(r/'tests/fixtures/weather.json')],check=True)

 # Geolocation is also required by the clock when the weather page is disabled.
 with tempfile.TemporaryDirectory(prefix='orbit-auto-location-off-') as d:
  w=Path(d)
  for name in ['Arduino.h','WiFi.h','WiFiClientSecure.h','HTTPClient.h','esp_heap_caps.h','freertos/FreeRTOS.h','freertos/task.h','freertos/queue.h']:
   p=w/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "weather_platform.h"\n')
  flags=['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g','-DORBIT_LIVE=1','-DORBIT_BUILD_CHECK=1','-DORBIT_WEATHER_ENABLED=0','-I'+str(w),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3')]
  b=w/'off';subprocess.run(flags+[str(r/'tests/auto_location_worker_test.cpp'),str(r/'firmware/OrbitS3/weather.cpp'),str(r/'firmware/OrbitS3/timezone_data.cpp'),'-o',str(b)],check=True);subprocess.run([str(b),str(r/'tests/fixtures/weather.json')],check=True)
