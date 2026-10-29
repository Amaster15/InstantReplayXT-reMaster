# Embedded by package_installers.py in a self-contained shell installer.
import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description='Install Instant Replay XT for native Linux OBS 31+ (current user; no sudo).')
    parser.add_argument('--dry-run', action='store_true', help='Show destination without changing files')
    parser.add_argument('--uninstall', action='store_true', help='Remove only files shipped by this installer; preserve OBS settings and backups')
    parser.add_argument('--install-dir', type=Path, help='Custom absolute plugin folder ending in InstantReplay; must be in an OBS plugin search location')
    args = parser.parse_args()
    if platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('This package requires Linux x86_64.')
    config = Path(os.environ.get('XDG_CONFIG_HOME') or str(Path.home() / '.config'))
    if not config.is_absolute():
        parser.error('XDG_CONFIG_HOME must be an absolute path.')
    obs_config = config / 'obs-studio'
    default_target = obs_config / 'plugins' / 'InstantReplay'
    target = args.install_dir or default_target
    if not target.is_absolute() or target.name != 'InstantReplay' or '..' in target.parts:
        parser.error('Choose an absolute dedicated plugin folder ending in InstantReplay (no .. components).')
    if not args.uninstall and target != default_target and (default_target / 'bin/64bit/InstantReplay.so').exists():
        parser.error('Uninstall the standard copy before choosing a different folder, to avoid duplicate loading.')
    if target.is_symlink():
        parser.error('The plugin destination is a symlink; refusing to modify it.')
    if args.install_dir:
        print('Custom location: OBS must be configured to search its parent folder. This is not the replay video folder.')
    print(('Uninstall from: ' if args.uninstall else 'Install into: ') + str(target))
    if args.dry_run:
        return
    if os.geteuid() == 0:
        parser.error('Run as your normal desktop user, without sudo, so OBS finds the plugin in your user configuration.')
    if not shutil.which('pgrep'):
        parser.error('Install procps (pgrep) so the installer can check whether OBS is running.')
    running = subprocess.run(['pgrep', '-u', str(os.getuid()), '-x', 'obs'], stdout=subprocess.DEVNULL)
    if running.returncode == 0:
        parser.error('Close OBS Studio before installing or uninstalling.')
    if running.returncode != 1:
        parser.error('Could not check whether OBS is running.')
    payload = base64.b64decode(PAYLOAD)
    if hashlib.sha256(payload).hexdigest() != PAYLOAD_SHA256:
        parser.error('Installer payload checksum failed.')
    files = {}
    with tarfile.open(fileobj=io.BytesIO(payload), mode='r:gz') as archive:
        for item in archive.getmembers():
            if item.name in ('LICENSE', 'README.md', 'build-info.json'):
                name = item.name
            elif item.name.startswith('InstantReplay/'):
                name = item.name[len('InstantReplay/'):]
            else:
                continue
            if item.isdir():
                continue
            if not item.isfile() or Path(name).is_absolute() or '..' in Path(name).parts:
                parser.error('Invalid payload entry.')
            files[name] = archive.extractfile(item).read()
    if 'bin/64bit/InstantReplay.so' not in files or 'data/locale/en-US.ini' not in files:
        parser.error('Installer payload is incomplete.')
    if args.uninstall:
        for name in files:
            path = target / name
            if any(parent.is_symlink() for parent in path.parents if parent != target):
                parser.error('A destination parent is a symlink; refusing to uninstall.')
        for name in files:
            (target / name).unlink(missing_ok=True)
        # Never remove unrecognized files or another plugin's data.
        for folder in ('bin/64bit', 'bin', 'data/locale', 'data', ''):
            try:
                (target / folder).rmdir()
            except (FileNotFoundError, OSError):
                pass
        print('Plugin removed. OBS settings, replay recordings and backups were preserved.')
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.InstantReplay-stage-', dir=target.parent))
    backup = None
    try:
        for name, data in files.items():
            path = stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o644)
        if target.exists():
            backup_root = obs_config / 'plugin-backups'
            backup_root.mkdir(parents=True, exist_ok=True)
            backup = backup_root / ('InstantReplay-' + str(time.time_ns()))
            shutil.move(str(target), str(backup))
        try:
            stage.rename(target)
        except Exception:
            if backup:
                shutil.move(str(backup), str(target))
            raise
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    if backup:
        print('Previous installation backed up to: ' + str(backup))
    print('Installed. Restart OBS and open Tools > Instant Replay XT Settings.')
    print('For native OBS installations; this package is not a Flatpak runtime build.')


if __name__ == '__main__':
    main()
