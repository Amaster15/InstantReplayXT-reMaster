"""Test waiting, cancellation and verification without installing a real plugin."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='irxt update ') as temp:
    root = Path(temp)
    installer = root / 'installer.run'
    installer.write_text('#!/bin/sh\nprintf installed > "$2/result"\n')
    marker = root / 'pending'
    status = root / 'update.txt'
    digest = hashlib.sha256(installer.read_bytes()).hexdigest()
    if os.name == 'nt':
        command = ['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'updater/install-after-obs.ps1')]
    else:
        command = [sys.executable, str(ROOT / 'updater/install-after-obs.py')]
    def args(expected, pid):
        return [str(installer), expected, str(pid), str(root), str(marker), str(status)]
    # A missing marker means cancelled; never execute an installer.
    subprocess.run(command + args(digest, 2147483647), check=True, timeout=15)
    assert not status.exists() and not (root / 'result').exists()
    # Reject changed bytes, even after the download was initially verified.
    marker.touch()
    subprocess.run(command + args('0' * 64, 2147483647), check=True, timeout=15)
    message = status.read_text(encoding='utf-8-sig')
    assert 'checksum' in message.lower(), message
    assert not marker.exists() and not (root / 'result').exists()
    # Wait for an actual process, then cancel without running the installer.
    marker.touch()
    helper = subprocess.Popen(command + args(digest, os.getpid()))
    try:
        time.sleep(1.5)
        assert helper.poll() is None
        marker.unlink()
        assert helper.wait(timeout=10) == 0
        assert not (root / 'result').exists()
    finally:
        if helper.poll() is None:
            helper.terminate(); helper.wait()
    if os.name != 'nt':
        # End-to-end detached Linux handoff into a destination containing spaces.
        sleeper = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(1)'])
        marker.touch()
        helper = subprocess.Popen(command + args(digest, sleeper.pid))
        sleeper.wait()
        assert helper.wait(timeout=10) == 0
        assert (root / 'result').read_text() == 'installed'
        assert 'installed' in status.read_text()
print('Update helper: cancellation, parent wait and checksum verification passed.')
