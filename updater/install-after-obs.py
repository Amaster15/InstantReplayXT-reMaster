#!/usr/bin/env python3
"""Wait for the originating OBS process, verify the installer, then upgrade."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    installer, expected, pid_text, target, marker_text, status_text = sys.argv[1:]
    marker, status = Path(marker_text), Path(status_text)
    pid = int(pid_text)
    deadline = time.monotonic() + 1800
    while marker.exists():
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            break
        if time.monotonic() >= deadline:
            status.write_text('Update expired: OBS was not closed within 30 minutes.')
            marker.unlink(missing_ok=True)
            return
        time.sleep(1)
    if not marker.exists():
        return
    try:
        if hashlib.sha256(Path(installer).read_bytes()).hexdigest() != expected:
            raise RuntimeError('Installer checksum changed; installation cancelled.')
        with status.with_suffix('.log').open('w') as log:
            result = subprocess.run(['sh', installer, '--install-dir', target], stdout=log, stderr=subprocess.STDOUT)
        status.write_text('Update installed. Restart OBS.' if result.returncode == 0 else 'Update failed. See update.log in the download folder.')
    except Exception as error:
        status.write_text('Update failed: ' + str(error))
    finally:
        marker.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
