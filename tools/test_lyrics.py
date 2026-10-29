"""Actual bounded parser and worker with synthetic lyrics/fake HTTPS; never calls lyric APIs."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='orbit-lyrics-') as d:
 w=Path(d)
 for name in ['Arduino.h','WiFi.h','WiFiClientSecure.h','HTTPClient.h','esp_heap_caps.h','freertos/FreeRTOS.h','freertos/task.h','freertos/queue.h']:
  p=w/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n#include "lyrics_platform.h"\n')
 flags=['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-g','-DORBIT_LIVE=1','-DORBIT_BUILD_CHECK=1','-I'+str(w),'-I'+str(r/'tests/fakes'),'-I'+str(r/'.pio/libdeps/orbit-live-check/ArduinoJson/src'),'-I'+str(r/'firmware/OrbitS3')]
 for name,sources in [('parser',['tests/lyrics_parser_test.cpp']),('worker',['tests/lyrics_worker_test.cpp','firmware/OrbitS3/lyrics.cpp'])]:
  binary=w/name;subprocess.run(flags+[str(r/p) for p in sources]+[str(r/'firmware/OrbitS3/lyrics_parser.cpp'),str(r/'firmware/OrbitS3/metadata_text.cpp'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
