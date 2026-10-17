"""Run the production desktop UI against bounded, populated local fixtures."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="tab5-sd-") as directory:
    for index in range(260):
        Path(directory, f"{index:03d}-long-telemetry-capture-file-name-for-scroll-testing.csv").touch()
    environment = dict(os.environ, SDL_VIDEODRIVER="dummy", M5TAB5_DESKTOP_MAX_FRAMES="8",
                       M5TAB5_DESKTOP_ASSERT_LAUNCHER_UI="1", M5TAB5_DESKTOP_ASSERT_ALL_APPS_UI="1",
                       M5TAB5_DESKTOP_WIFI_MANY_NETWORKS="1", M5TAB5_DESKTOP_SD_ROOT=directory)
    sys.exit(subprocess.run([sys.argv[1]], env=environment, timeout=60).returncode)
