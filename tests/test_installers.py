"""Exercise install, upgrade and uninstall on disposable destinations."""
import hashlib
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / 'artifacts'


def check_linux():
    installer = ARTIFACTS / 'InstantReplayXT-2.0.10-obs31-linux-x64-install.run'
    with tempfile.TemporaryDirectory(prefix='irxt-install-test-') as temp:
        config = Path(temp) / 'config with spaces'
        env = dict(os.environ, XDG_CONFIG_HOME=str(config))
        target = config / 'obs-studio/plugins/InstantReplay'
        def run(*args, success=True):
            p = subprocess.run(['sh', str(installer), *args], env=env, text=True, capture_output=True)
            if success and p.returncode != 0:
                raise AssertionError(p.stdout + p.stderr)
            if not success and p.returncode == 0:
                raise AssertionError('Expected installer to refuse this operation')
            return p
        run('--dry-run')
        assert not config.exists(), 'Dry run changed files'
        run()
        binary = target / 'bin/64bit/InstantReplay.so'
        assert binary.read_bytes()[:4] == b'\x7fELF'
        assert (target / 'data/locale/en-US.ini').is_file()
        assert (target / 'data/updater/install-after-obs.py').is_file()
        installed_digest = hashlib.sha256(binary.read_bytes()).hexdigest()
        binary.write_bytes(b'old plugin')
        run()
        backups = list((config / 'obs-studio/plugin-backups').iterdir())
        assert len(backups) == 1
        assert (backups[0] / 'bin/64bit/InstantReplay.so').read_bytes() == b'old plugin'
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == installed_digest
        sentinel = target / 'keep-me.txt'
        sentinel.write_text('not owned by installer')
        obs_settings = config / 'obs-studio/global.ini'
        obs_settings.write_text('preserve settings')
        run('--uninstall')
        assert not binary.exists()
        assert sentinel.read_text() == 'not owned by installer'
        assert obs_settings.read_text() == 'preserve settings'
        assert backups[0].exists()
        sentinel.unlink()
        target.rmdir()
        custom = Path(temp) / 'custom plugins with spaces' / 'InstantReplay'
        run('--install-dir', str(custom), '--dry-run')
        assert not custom.exists()
        run('--install-dir', str(custom))
        custom_binary = custom / 'bin/64bit/InstantReplay.so'
        assert hashlib.sha256(custom_binary.read_bytes()).hexdigest() == installed_digest
        run('--install-dir', str(custom))
        assert hashlib.sha256(custom_binary.read_bytes()).hexdigest() == installed_digest
        run('--install-dir', str(custom), '--uninstall')
        assert not custom_binary.exists()
        run('--install-dir', str(Path(temp)), success=False)
        run('--install-dir', 'relative/InstantReplay', success=False)

        external = Path(temp) / 'external'
        external.mkdir()
        target.symlink_to(external, target_is_directory=True)
        run(success=False)
        assert not list(external.iterdir())
    print('Linux: dry-run, paths with spaces, install, backup/upgrade, uninstall, settings preservation and symlink refusal passed.')


def check_windows():
    # This writes ProgramData only on the disposable GitHub Actions test runner.
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise RuntimeError('Windows installation smoke test is restricted to a disposable CI runner')
    setup = ARTIFACTS / 'InstantReplayXT-2.0.10-obs31-windows-x64-setup.exe'
    target = Path(os.environ['ProgramData']) / 'obs-studio/plugins/InstantReplay'
    if target.exists():
        raise RuntimeError('Test destination already exists; refusing to overwrite it')
    quiet = ['/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART']
    subprocess.run([str(setup), *quiet], check=True)
    dll = target / 'bin/64bit/InstantReplay.dll'
    with zipfile.ZipFile(ARTIFACTS / 'InstantReplayXT-2.0.10-obs31-windows-x64.zip') as z:
        assert dll.read_bytes() == z.read('InstantReplay/bin/64bit/InstantReplay.dll')
    assert (target / 'data/locale/en-US.ini').is_file()
    assert (target / 'data/updater/install-after-obs.ps1').is_file()
    subprocess.run([str(setup), *quiet], check=True)
    backup_root = target.parents[1] / 'plugin-backups'
    assert list(backup_root.glob('InstantReplay-*/InstantReplay.dll'))
    sentinel = target / 'keep-me.txt'
    sentinel.write_text('not owned by installer')
    uninstaller = target / 'uninstall/unins000.exe'
    assert uninstaller.is_file()
    subprocess.run([str(uninstaller), *quiet], check=True)
    assert not dll.exists()
    assert sentinel.exists()
    with tempfile.TemporaryDirectory(prefix='irxt-custom-') as temp:
        custom = Path(temp) / 'custom plugins with spaces' / 'InstantReplay'
        subprocess.run([str(setup), *quiet, '/DIR=' + str(custom)], check=True)
        assert (custom / 'bin/64bit/InstantReplay.dll').is_file()
        # Upgrade without /DIR must reuse the custom destination.
        subprocess.run([str(setup), *quiet], check=True)
        assert (custom / 'data/locale/en-US.ini').is_file()
        assert not dll.exists()
        subprocess.run([str(custom / 'uninstall/unins000.exe'), *quiet], check=True)
        assert not (custom / 'bin/64bit/InstantReplay.dll').exists()
    print('Windows: standard ProgramData path, installed DLL hash, upgrade backup and uninstall preservation passed.')


if __name__ == '__main__':
    if platform.system() == 'Windows':
        check_windows()
    else:
        check_linux()
