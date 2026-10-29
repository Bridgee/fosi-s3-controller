"""Exercise the actual one-shot ESP32 trigger with mocked serial/TLS/HTTP; no live push."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='orbit-iphone-') as d:
 w=Path(d)
 for name in ['Arduino.h','WiFi.h','WiFiClientSecure.h','HTTPClient.h','esp_heap_caps.h','esp_system.h','freertos/FreeRTOS.h','freertos/task.h']:
  p=w/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "iphone_trigger_platform.h"\n')
 for key in ['HOST_TEST_DEVICE_KEY','']:
  binary=w/('trigger-configured' if key else 'trigger-empty')
  subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g','-DORBIT_LIVE=1','-DORBIT_BUILD_CHECK=1','-DORBIT_IPHONE_TRIGGER_ENABLED=1','-DORBIT_IPHONE_SERIAL_TEST=1','-DORBIT_BARK_DEVICE_KEY="'+key+'"','-I'+str(w),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3'),str(r/'firmware/OrbitS3/iphone_trigger.cpp'),str(r/'tests/iphone_trigger_test.cpp'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)]+([] if key else ['empty']),check=True)
