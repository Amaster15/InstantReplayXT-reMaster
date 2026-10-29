# Instant Replay XT — OBS 31+ port

Native C++ / Qt 6 port of [spreadfire/InstantReplayXT](https://github.com/spreadfire/InstantReplayXT),
based on commit `25807e749a3c4e8e1ad83790d323e3db3ff0718d`.
Original authors: A Docile Sloth and spreadfire1. Original GPL license retained in LICENSE.

**Target: Windows 10/11 x64 and Linux x86_64, OBS Studio 31.0.0 and later.**
OBS 32 on Windows 10 is the primary user test target. This is a development port,
not yet a release validated with live replay playback on both operating systems.
Compatibility with future OBS major versions must be rechecked; “31+” is not an unlimited guarantee.

## Current verification

| Check | Result |
| --- | --- |
| Build/link with genuine OBS 31.0.0 SDK on Linux | Passed |
| Controller integration tests with OBS 31.0.0 | Passed |
| Shared module load/unload and Qt dialog lifecycle with OBS 31.0.0 | Passed in a test frontend |
| Build and tests with installed OBS 32.2.2 on Linux | Passed |
| Compile/API check against OBS 33.0.0 headers | Passed |
| Windows x64 native build | Passed on Windows Server 2022 with MSVC 19.44, Qt 6.6.3; DLL dependencies and exports verified |
| Windows 10 + OBS 32 live replay | Pending |
| Ubuntu 24.04 baseline package build | Passed with Qt 6.4.2; controller and module lifecycle tests passed |

Local tests use Qt 6.11.2. Those local binaries are **not portable OBS 31 release packages**:
older OBS installations may have older Qt. Release builds use OBS 31's own Windows Qt
dependencies, or Ubuntu 24.04's Qt 6.4 on Linux. The source builds with newer Qt for development.

## What changed

- Qt 6/C++17 and real SDK headers replace Qt 5 and the vendored 2017 API header.
- OBS's replay-saved event and UI timers replace the endless polling thread and sleeps.
- One hotkey saves and plays the replay; repeated triggers during a replay are ignored.
- Correct timeout units and clamp short replay durations to prevent enormous waits.
- Preserve scene/source references, check configuration, and restore prior global audio mute states.
- Manual scene changes and scene collection cleanup cancel the replay safely.
- Backups run off the UI thread, preserve existing filenames, and handle UTF-8 paths on Windows/Linux.
- Clean up timers, callbacks, hotkeys and the settings dialog on unload.
- Keep the original `InstantReplay` and `replayCallout_data` scene collection settings keys.

## Build a Windows x64 package

Prerequisites on Windows: Git, Python 3.10+, CMake 3.28+, and Visual Studio 2022 with
**Desktop development with C++**, the MSVC x64/x86 tools and a Windows SDK.
Run from a checkout/extracted source folder:

```powershell
python scripts/build.py
```

The script downloads the pinned official OBS 31.0.0 source, verifies its Git commit,
builds its SDK, then builds the plugin. OBS's bootstrap downloads and verifies its
2024-09-12 dependency bundles, including Qt. The first build requires internet access
and several GB of disk space. Output:

```text
artifacts/InstantReplayXT-2.0.0-obs31-windows-x64.zip
artifacts/InstantReplayXT-2.0.0-obs31-windows-x64.zip.sha256
```

Before packaging, the script loads the compiled module to verify native dependencies,
required entry points and its OBS 31 API version. This does not start replay playback.
Only the plugin, locale, documentation and build metadata are packaged. OBS/Qt runtime
DLLs are not bundled or replaced. The installed OBS application provides those runtimes.
The GitHub Actions Windows job runs the same script. The initial Windows and Linux packages were built successfully in
[GitHub Actions run 1](https://github.com/Amaster15/InstantReplayXT-reMaster/actions/runs/40807018374).
Download the platform artifact from that run and extract its inner ZIP or tar.gz package.
Both artifact and package SHA256 checksums were verified after download.
Live replay playback in Windows 10/OBS 32 remains to be tested.

## Build a Linux baseline package

Use Ubuntu 24.04 x86_64 with Qt 6.4. Install the dependencies listed in
[the workflow](.github/workflows/build.yml), then run:

```sh
xvfb-run -a python3 scripts/build.py --test
```

Use `python3 scripts/build.py --test` instead if a working X display is already available.
The script builds the actual OBS 31 SDK; it does not relabel a binary built against a newer SDK.
It stages a `.tar.gz` and SHA256 checksum under `artifacts/`. Linux baseline packaging
rejects Qt 6.5+ to prevent accidental dependence on newer Qt symbols.
Ubuntu-built packages still depend on compatible distro C/C++/Qt libraries. Flatpak and
other sandboxed OBS distributions require builds for their matching runtime.

For a local development build using an installed OBS 31+ SDK and Qt 6.4+:

```sh
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release -DIRXT_BUILD_TESTS=ON
cmake --build build-local --parallel
ctest --test-dir build-local --output-on-failure
cmake --install build-local --prefix "$PWD/dist"
```

`make` remains a Linux fallback. CMake tests include both controller and module lifecycle
coverage; `make test` runs the controller test only. All integration tests currently run
on Linux because the fake frontend relies on ELF symbol interposition.

## Install on Windows 10/11

Close OBS, then run `InstantReplayXT-2.0.0-obs31-windows-x64-setup.exe`.
Accept the Windows administrator prompt. The installer chooses the standard location:

```text
C:\ProgramData\obs-studio\plugins\InstantReplay
```

Restart OBS and open **Tools → Instant Replay XT Settings**. Uninstall through Windows
**Settings → Apps → Instant Replay XT for OBS**. OBS scenes, profiles and recordings
are preserved. Upgrades back up the previous DLL under
`%ProgramData%\obs-studio\plugin-backups`.

If an older DLL exists inside `C:\Program Files\obs-studio\obs-plugins\64bit`, setup
asks you to move it out first to prevent duplicate plugin loading. For a custom OBS
installation path, also check that installation's old plugin folder yourself.
The installer is unsigned, so Windows may show an unknown-publisher warning.

Portable OBS uses a different search path. Use the ZIP package instead, placing its
DLL in `<portable OBS>/obs-plugins/64bit/` and its locale in
`<portable OBS>/data/obs-plugins/InstantReplay/locale/`.

## Install on Linux

Close OBS and run the self-contained installer as your desktop user, **without sudo**:

```sh
sh InstantReplayXT-2.0.0-obs31-linux-x64-install.run
```

It installs to `${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/InstantReplay`.
Python 3 and `pgrep` (the `procps` package on Ubuntu) are required. Paths containing
spaces are supported. The installer verifies its payload checksum before modifying files.
Existing installations are backed up under the OBS configuration's `plugin-backups`
directory, outside the plugin search path. Restart OBS after installation.

Preview the destination, or uninstall later using the same downloaded file:

```sh
sh InstantReplayXT-2.0.0-obs31-linux-x64-install.run --dry-run
sh InstantReplayXT-2.0.0-obs31-linux-x64-install.run --uninstall
```

Uninstall removes only files shipped by the installer; OBS settings, recordings,
backups and unrecognized files are preserved. This Ubuntu 24.04 / Qt 6.4 package targets
native Linux x86_64 OBS with compatible system libraries. It is **not a Flatpak runtime
build**. Portable OBS and sandboxed distributions should use a package built for their
runtime rather than this installer.

The manual `.zip` and `.tar.gz` packages remain available. Installer generation uses
`scripts/package_installers.py --platform windows` (requires Inno Setup 6) or
`--platform linux`. The workflow tests install, upgrade backup and uninstall on a
Windows runner and in a temporary Linux configuration directory.

Both standard install layouts work with OBS 31/32 and the legacy loader in OBS 33.
OBS 34 packaging must be revalidated before claiming support.

## Configure and use

1. Enable Replay Buffer in OBS Output settings and start it.
2. Create a replay scene containing a **Media Source**, with audio routed as desired.
3. In **Tools → Instant Replay XT Settings**, enable the plugin and enter the exact
   replay scene and Media Source names.
4. Set Write Pause to the maximum save timeout (e.g. 10 seconds; zero means one second).
   Set Transition Pause in milliseconds and Replay Length in seconds.
5. Optionally enable global desktop/mic muting and select a backup folder.
6. Bind **Instant Replay XT: Save and play** in Settings → Hotkeys. Remove that same
   binding from OBS's **Save Replay** action; this plugin now requests the save itself.
7. Trigger from a different scene. The replay plays and then returns to the original scene.
   A manual scene change cancels the automatic return.

Replay duration remains timer-based, not media-end detection. The Media Source must
belong to the replay scene. Muting covers global Desktop Audio/Mic-Aux channels, not
arbitrary application audio sources. Avoid another independent replay-save request
while this action waits; OBS's saved event does not identify its requesting caller.
Pending backups finish on unload, so slow storage can delay exit.

## Before release

Validate actual replay media, audio, transitions/stingers, Studio Mode, restart/hotkey
persistence, missing sources, Unicode backup folders, and exit/collection changes during
replay on Windows 10 + OBS 32 and Linux. Use a test scene collection. Repeat on OBS 31 to
establish the minimum runtime and OBS 33 to check forward compatibility.

The tests exercise real libobs source/data ownership and real Qt objects, but their fake
frontend and media source do not substitute for a full OBS playback test.
