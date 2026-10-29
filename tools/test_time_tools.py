"""Production time tools and NVS settings with deterministic clocks, no audio/I/O."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='orbit-time-') as d:
 b=Path(d)/'time-tools'
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(r/'tests/fakes'),'-I'+str(r/'firmware/OrbitS3'),str(r/'tests/time_tools_test.cpp'),str(r/'firmware/OrbitS3/time_tools.cpp'),str(r/'firmware/OrbitS3/timezone_data.cpp'),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
