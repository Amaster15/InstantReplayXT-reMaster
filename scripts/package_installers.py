#!/usr/bin/env python3
"""Create installers from the verified platform packages (no plugin rebuild)."""
import argparse
import base64
import hashlib
from pathlib import Path
import shutil
import subprocess
import os
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('windows', 'linux'), required=True)
    parser.add_argument('--iscc', help='Path to Inno Setup 6 ISCC.exe')
    args = parser.parse_args()
    name = 'InstantReplayXT-2.0.13-obs31-' + args.platform + '-x64'
    artifacts = ROOT / 'artifacts'
    package = artifacts / (name + ('.zip' if args.platform == 'windows' else '.tar.gz'))
    payload = package.read_bytes()
    expected = Path(str(package) + '.sha256').read_text().split()[0]
    if hashlib.sha256(payload).hexdigest() != expected:
        raise RuntimeError('Platform package checksum failed')
    if args.platform == 'linux':
        output = artifacts / (name + '-install.run')
        template = (ROOT / 'installers/linux_installer.py').read_text()
        embedded = 'PAYLOAD = ' + repr(base64.b64encode(payload).decode()) + '\n'
        embedded += 'PAYLOAD_SHA256 = ' + repr(expected) + '\n'
        output.write_text('#!/bin/sh\ncommand -v python3 >/dev/null 2>&1 || { echo "Python 3 is required." >&2; exit 1; }\nexec python3 - "$@" <<\'IRXT_PYTHON\'\n' + embedded + template + '\nIRXT_PYTHON\n')
        output.chmod(0o755)
    else:
        stage = ROOT / 'build-installer-windows' / 'package'
        if stage.exists():
            shutil.rmtree(stage)
        stage.mkdir(parents=True)
        with zipfile.ZipFile(package) as archive:
            for name_in_zip in archive.namelist():
                path = Path(name_in_zip)
                if path.is_absolute() or '..' in path.parts or '\\' in name_in_zip:
                    raise RuntimeError('Invalid package path')
            archive.extractall(stage)
        candidates = [args.iscc, shutil.which('iscc'),
                      str(Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Inno Setup 6/ISCC.exe'),
                      str(Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Inno Setup 6/ISCC.exe')]
        iscc = next((p for p in candidates if p and Path(p).is_file()), None)
        if not iscc:
            raise RuntimeError('Install Inno Setup 6 or pass --iscc PATH')
        subprocess.run([iscc, '/DPackageDir=' + str(stage), '/DOutputDir=' + str(artifacts),
                        str(ROOT / 'installers/windows.iss')], check=True)
        output = artifacts / 'InstantReplayXT-2.0.13-obs31-windows-x64-setup.exe'
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    Path(str(output) + '.sha256').write_text(digest + '  ' + output.name + '\n')
    print('Installer:', output)


if __name__ == '__main__':
    main()
