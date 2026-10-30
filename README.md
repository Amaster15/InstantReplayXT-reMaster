# Instant Replay XT — OBS 31+ port

Native C++ / Qt 6 port of [spreadfire/InstantReplayXT](https://github.com/spreadfire/InstantReplayXT),
based on commit `25807e749a3c4e8e1ad83790d323e3db3ff0718d`.
Original authors: A Docile Sloth and spreadfire1. Original GPL license retained in LICENSE.

**Target: Windows 10/11 x64 and Linux x86_64, OBS Studio 31.0.0 and later.**
OBS 32 on Windows 10 is the primary user test target; the user reported successful live testing of version 2.0.3.
Linux has automated build, controller, settings and installer coverage; live playback validation on Linux is still pending.
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
| Windows 10 + OBS 32 live use | User reported all tested 2.0.3 features working |
| Ubuntu 24.04 baseline package build | Passed with Qt 6.4.2; controller and module lifecycle tests passed |

Local tests use Qt 6.11.2. Those local binaries are **not portable OBS 31 release packages**:
older OBS installations may have older Qt. Release builds use OBS 31's own Windows Qt
dependencies, or Ubuntu 24.04's Qt 6.4 on Linux. The source builds with newer Qt for development.

## What changed

- Qt 6/C++17 and real SDK headers replace Qt 5 and the vendored 2017 API header.
- OBS's replay-saved event and UI timers replace the endless polling thread and sleeps.
- Separate Save replay and Play latest replay hotkeys; automatic switching is optional and defaults off.
- Correct timeout units and clamp short replay durations to prevent enormous waits.
- Preserve scene/source references, check configuration, and restore prior global audio mute states.
- Manual scene changes and scene collection cleanup cancel the replay safely.
- Replay files and backups are written off the UI thread, with UTF-8 paths on Windows/Linux.
- The main clip is `InstantReplay` plus the OBS container extension (for example, `InstantReplay.mkv`), replaced on each save. Backups use `InstantReplay_1.mkv`, `InstantReplay_2.mkv`, etc., continuing after the highest existing number.
- Browse buttons select replay and backup folders. A blank replay folder uses OBS's save directory.
- Saving works even with incomplete playback settings. The settings dialog and OBS log report stopped buffers, missing playback configuration, timeouts, and file errors.
- Clean up timers, callbacks, hotkeys and the settings dialog on unload.
- Keep the original `InstantReplay` and `replayCallout_data` scene collection settings keys.

## Replay player choice and icons (2.0.9)

The Clip Library has a **Replay player** dropdown:

- **Standard Media Source** keeps the existing OBS player. Changing speed reloads the clip and restores the position; the note under the slider explains this.
- **VLC live slow motion** changes playback rate without stopping, reopening or seeking the clip. Install **64-bit VLC 3.x** on Windows, or VLC/libVLC 3.x with its codec plugins on Linux, then restart OBS. VLC 4 is not supported by this player yet. VLC is optional for standard playback and is not bundled in the installer.

Configure the normal replay scene and Media Source first. With no replay/save in progress, selecting VLC creates an **Instant Replay VLC Player** named after that source with ` (VLC live)` appended. The standard source must be directly in the replay scene, outside groups. The new player copies the placement, crop, filters and audio routing at creation. The dropdown toggles which player is visible and remembers the choice per scene collection; the original source remains available. Select Standard to switch back. Only the selected player is used by replay hotkeys and the clip library; the BRB playlist is unchanged. This VLC player outputs stereo audio.

Clip rows now use a **play triangle** for Play and a **screen with play triangle** for Play + switch scene. Hover for tooltips. Their behavior remains independent of the global scene-switch checkbox.

Validation covers real VLC video/audio decoding, correct display dimensions, continuous position through rate changes, paused rate changes, fixed-file release, mode switching and controller rate calls without reloads. The synthetic test clip in `tests/fixtures` was generated with FFmpeg testsrc2 and sine filters.

## Windows updater HTTPS fix (2.0.8)

Windows update checks and installer downloads now use the Windows system curl client with certificate verification, HTTPS-only redirects, time/size limits and the existing SHA256 verification. They no longer depend on OBS shipping Qt TLS plugins. This fixes “TLS initialization failed” on affected installations. Windows 10 version 1809 or newer includes this client.

If v2.0.6 or v2.0.7 shows this error, close OBS and manually install the v2.0.8 Windows EXE from Releases once. Future updates can use the Updates tab. Linux continues using Qt HTTPS. Windows CI checks release metadata and a real installer download with Qt plugin search paths disabled, including size and SHA256 verification.

## Clip row controls and backup scanning (2.0.7)

Each clip now has **Play** and **Play + switch scene** buttons. Play prepares the clip without changing scenes (or plays it if you are already on the replay scene). Play + switch scene plays it in the configured replay scene and returns after it ends. These row buttons ignore the checkbox; that checkbox still controls **Play selected clip** and double-click playback.

The library scans the single backup folder and every configured match backup folder, including disabled matches and nested subfolders. Duplicate file paths appear once. It refreshes while the tab is open; **Refresh clips** scans immediately. Directory symlinks are not followed during recursive scanning.

## Updates tab (2.0.6)

Install 2.0.6 manually once to get the **Updates** tab. Click **Check for updates** to check this repository's latest stable GitHub release, then **Download and install after OBS closes**. The plugin selects the Windows x64 EXE or Linux x86_64 installer, verifies the size and GitHub SHA256 digest, and queues setup for the current plugin folder. No update checks or downloads happen automatically.

Finish your stream/recording and close OBS normally within 30 minutes. The helper waits for that OBS process to exit, verifies the downloaded file again, then runs the installer. Windows opens the installer with its normal administrator prompt; Linux runs the existing user installer and records the result in `update.txt` and `update.log`. Restart OBS after installation. **Open update downloads / log** shows the files; **Cancel update** cancels a download or a queued installation while OBS remains open. Other running OBS instances can prevent installation; the installer reports this rather than replacing loaded files.

The updater supports the standard `InstantReplay/bin/64bit` layout, including custom destinations selected by our installer. Other layouts must use the GitHub release downloads manually. The existing unsigned installer and native Linux library requirements still apply. Network errors, missing assets, and failed checksum verification leave the current plugin installed. Future releases must retain the platform asset naming convention and provide GitHub SHA256 digests.

## Clip library and slow motion (2.0.5)

Open **Tools > Instant Replay XT Settings > Clip library**. The list includes video files in the single backup folder and every configured match folder, including disabled matches, plus the current replay file. Clips are sorted newest first. Use **Refresh clips** or leave the tab open for automatic refresh. Removing a match from the configuration also removes its folder from this list; files remain on disk. Enable backups to preserve older clips: overwritten files cannot be recovered by the library.

Select a clip and click **Play selected clip**, or double-click it. **Switch to replay scene when playing a clip** is independent of automatic switching after a save. With the box off, a clip is prepared for manual entry into the configured replay scene; if already on that scene, it plays there. With the box on, playback switches to the replay scene and returns to the previous scene at the clip's end. A manual scene change cancels the automatic return. Library clips play to their actual end rather than the configured ReplayLength.

The **Playback speed** slider ranges from 25% to 200%; **Normal speed** restores 100%. It changes the replay Media Source, not the BRB VLC playlist or saved file. OBS 31+ reloads its Media Source to change speed, so this is not a seamless frame-accurate speed ramp: the plugin restores the previous position after reload, with a possible brief interruption. Changes are applied after a short slider debounce. Speed starts at 100% when scene-collection settings load. Replay must be enabled and the configured Media Source must be visible in the replay scene.

## Match folders and BRB VLC playlists (2.0.3)

Open **Tools > Instant Replay XT Settings > Matches / BRB playlist**:

1. Click **Add folder…** for each match. Create/select a folder, then edit the match/team name in the table.
2. Enable **Use match folders instead of the single backup folder**, and enable **Backup Replay File** on the Replay settings tab.
3. Select the first match and click **Use selected match**. Each save goes to that folder until you click **Next match**.
4. Disable unused rows with their checkbox; use Up/Down to order matches. Disabled rows are skipped. At the end, there is no active match and backups pause until you explicitly choose/add another. The main replay still saves. Removing rows never deletes files.
5. Enter the exact name of an existing OBS **VLC Video Source** used on the BRB scene. **Refresh VLC now** replaces its playlist with the active match's folder and restarts playback. It preserves loop/shuffle and other VLC options. Use a dedicated source for this managed playlist.
6. Optionally enable automatic refresh. Successful backups and match changes refresh the playlist while the source is off-screen; if visible (including preview), refresh waits until it is hidden. Manual refresh works immediately and can interrupt playback. When match mode is off, VLC uses the single backup folder.

Each match folder continues its own `InstantReplay_1`, `_2`, etc. numbering. Settings,
folder order, enabled flags, active match and VLC selection are saved per scene collection.
The VLC Video Source must already be available in OBS; this plugin does not install VLC.

For the main filename, 2.0.3 releases the configured replay Media Source and waits for OBS
to acknowledge its update before replacing `InstantReplay` plus the recording extension.
This addresses the source holding the file open on Windows. Saving while that source is
visible may briefly clear its picture. If another application still locks the file or writing
fails, the timestamped OBS original is retained. Status now reports the exact main and backup
paths separately. A successful backup can refresh VLC even if main-file replacement fails.

## Saving and playing independently (2.0.2)

Saving stays on your current scene by default. The existing save hotkey binding is retained,
now labelled **Instant Replay XT: Save replay**. Bind the new **Play latest replay** hotkey
separately, or click the matching button in the plugin settings. Hotkey/button playback
switches to the replay scene and returns to the previous scene after the configured duration.
Manually entering the replay scene plays the latest clip and leaves scene switching under
your control. Leaving it early cancels playback control and restores muted audio.

Check **Automatically switch to replay scene after saving** to restore the combined save/play
behavior. The checkbox defaults off, including upgrades. Playback of an existing clip does
not require Replay Buffer to be running. The latest file is also saved in the Media Source's
settings so it can play after restarting OBS. Configure the Media Source inside the replay scene.

## Using the replay hotkeys

1. In OBS Settings > Output, enable Replay Buffer and configure its duration and recording format.
2. Click **Start Replay Buffer** in OBS and let it collect some video.
3. Open Tools > Instant Replay settings and check **Enable**. Choose the replay folder with **Browse…** (blank uses OBS's recording folder). Optionally enable backups and browse to their folder.
4. Set the replay scene and Media Source names exactly as they appear in OBS. These are needed for automatic playback; saving still works without them.
5. Assign **Instant Replay XT: Save replay** and **Instant Replay XT: Play latest replay** in OBS Settings > Hotkeys. Save from your live scene. Play later using the playback hotkey, the plugin's **Play latest replay** button, or by manually entering the replay scene.

The status at the bottom of the plugin settings and log lines containing `[InstantReplayXT]`
explain the result. Existing numbered backups are retained. Once the fixed clip has been
written successfully, OBS's timestamped original is removed; on a write error the original
is retained. Changing recording format changes the extension, so a clip in the old format
is left untouched. Backups must be enabled and have a folder selected.

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
artifacts/InstantReplayXT-2.0.16-obs31-windows-x64.zip
artifacts/InstantReplayXT-2.0.16-obs31-windows-x64.zip.sha256
```

Before packaging, the script loads the compiled module to verify native dependencies,
required entry points and its OBS 31 API version. This does not start replay playback.
Only the plugin, locale, documentation and build metadata are packaged. OBS/Qt runtime
DLLs are not bundled or replaced. The installed OBS application provides those runtimes.
The GitHub Actions Windows job runs the same script. The initial Windows and Linux packages were built successfully in
[GitHub Actions run 1](https://github.com/Amaster15/InstantReplayXT-reMaster/actions/runs/40807018374).
Download the platform artifact from that run and extract its inner ZIP or tar.gz package.
Both artifact and package SHA256 checksums were verified after download.
The user reported successful live testing of the 2.0.3 features on Windows 10/OBS 32.

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

Close OBS, then run `InstantReplayXT-2.0.16-obs31-windows-x64-setup.exe`.
Accept the Windows administrator prompt. Setup includes a destination folder selector and remembers the previous install location for upgrades. The recommended default is:

```text
C:\ProgramData\obs-studio\plugins\InstantReplay
```

For a custom location, choose a dedicated folder ending in `InstantReplay` inside an OBS plugin search location you have configured. An arbitrary folder will not be discovered automatically. Uninstall before relocating an existing installation to avoid duplicate plugins. This setting controls the plugin files; choose video save folders inside OBS.

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
sh InstantReplayXT-2.0.16-obs31-linux-x64-install.run
```

It installs to `${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/InstantReplay`.
Python 3 and `pgrep` (the `procps` package on Ubuntu) are required. Paths containing
spaces are supported. The installer verifies its payload checksum before modifying files.
Existing installations are backed up under the OBS configuration's `plugin-backups`
directory, outside the plugin search path. Restart OBS after installation.

To use a custom plugin folder, pass `--install-dir "/absolute/OBS-plugin-search-folder/InstantReplay"` when installing, upgrading, or uninstalling. The parent must be a location OBS searches; this does not configure OBS's search paths. Uninstall the default copy before relocating. The default needs no option.

Preview the destination, or uninstall later using the same downloaded file:

```sh
sh InstantReplayXT-2.0.16-obs31-linux-x64-install.run --dry-run
sh InstantReplayXT-2.0.16-obs31-linux-x64-install.run --uninstall
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
6. Bind **Instant Replay XT: Save replay** in Settings → Hotkeys. Remove that same
   binding from OBS's **Save Replay** action; this plugin now requests the save itself.
7. Save without switching, then trigger Play latest replay from a different scene to play and return.
   Alternatively enter the replay scene manually, or enable automatic switching after saving.
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

## Crash recovery and source selectors (2.0.10)

Settings are written as you edit to OBS's plugin configuration directory, in `InstantReplay/settings.json`, independently of OBS's scene-collection save. Writes use atomic replacement and keep the previous valid document in `settings.json.bak`. The config keeps separate entries for each scene-collection name and takes precedence over that collection's older embedded plugin settings at startup. If no separate config exists, existing OBS settings are used. A write failure appears at the bottom of the settings window; hovering that message shows the config path. `Save settings` is available from every tab. Hotkey bindings remain managed by OBS.

For a standard Windows OBS installation the file is `%APPDATA%/obs-studio/plugin_config/InstantReplay/settings.json`; on native Linux it is normally `~/.config/obs-studio/plugin_config/InstantReplay/settings.json` (OBS portable mode and XDG settings can change this). Copy both files to back up your plugin settings.

Choose the replay scene from the OBS scene list, then choose its standard Media Source from the replay source list. Only Media Sources directly in the selected scene are shown, because grouped sources are not supported for switching player modes. The BRB selector lists OBS VLC Video Sources. Lists refresh when opened and retain unavailable saved names rather than silently selecting another source. VLC player choice and playback speed are restored too; a missing generated VLC player is recreated when the standard source is available.

## VLC default and immediate clip changes (2.0.11)

VLC live slow motion is first in the player list and is selected on first use or the first upgrade to this version. You can still select Standard Media Source; explicit choices made in this version are saved. VLC 3.x is required for VLC mode (64-bit on Windows).

Play and Play + switch now stop the current replay and restart the chosen clip immediately, including clicking the same clip again. Old playback timers and pending speed seeks are cancelled. Play + switch preserves the original return scene across replacement clips; Play stays on the current scene or arms the selected replay if its scene is hidden. A missing clip does not interrupt current playback. Player-mode changes also stop an active replay rather than rejecting the switch. Save/file-replacement operations remain protected until they finish.

Each Play action checks the selected player and scene setup, creating the VLC player when needed without requiring an OBS restart. Config-file recovery, source dropdowns and installers from 2.0.10 remain included.

## Speed presets and 1% slow motion (2.0.12)

The speed slider and VLC source properties now support 1%–200%. Five quick buttons select 1%, 25%, 50%, 75%, or 100%; 100% restores normal speed. These use the same live VLC rate control as the slider and persist through the existing config system. At 1%, a 10-second clip takes about 16 minutes 40 seconds to play; this slows existing frames rather than generating intermediate frames.


## Reliable live 1% speed (2.0.13)

VLC 3 clamps its native live rate below 3.125%, despite reporting the requested value. The custom player now paces VLC frame stepping at 1–3%, so switching down from 100% or 50% works without reopening or seeking the clip. Returning to 4% or above resumes native continuous playback. Pause remains pause when changing speeds.

Audio is silent during 1–3% frame stepping. Frame timing uses the clip's reported frame rate (30 fps fallback); variable-frame-rate clips may have approximate timing. At 1%, a 30 fps clip advances about one frame every 3.33 seconds, so long frame holds are expected.

Regression coverage counts distinct decoded pictures during both 100% → 1% and 50% → 1% transitions, initial 1% playback, and pause/resume. It no longer relies on VLC's reported rate as proof of actual slow motion.

The clip library also has a **Pause / Resume** button next to the speed presets. Pause holds the current frame and suspends any timed replay scene return until you resume. It works with VLC and Standard Media Source, including 1% playback. Starting another clip clears the previous freeze.


## Backward and forward scrubbing (2.0.14)

The clip library now has a **Clip position** timeline with elapsed and total time. Drag left to an earlier moment or right to a later moment. This seeks within the video; it does not continuously play the clip in reverse. Scrubbing a paused clip updates its freeze frame and keeps it paused. A playing clip continues at the selected speed.

The timeline follows playback when you are not dragging, throttles seek requests during a drag, and applies the final position on release. It disables itself when no playable clip is available. Delayed requests from a previous clip are ignored, and timed replay scene return adjusts to the new position. Tests cover forward/backward paused VLC seeks, timeline controls, stale clip protection, and timer adjustment.


## Smoother scrubbing (2.0.15)

Dragging the clip timeline temporarily pauses playback, then resumes on release
if the clip was playing. Existing freeze frames stay paused. Preview requests
are sent at most every 40 ms, with the newest slider position taking priority.
VLC no longer receives an extra frame-step command for every seek, allowing it
to combine obsolete seek requests instead of building up work during a drag.
A single delayed frame refresh restores paused previews after a burst of seeks.
The release position is always sent immediately.

Backward preview responsiveness still depends on video resolution, keyframe
spacing and decoding speed; this is not a decoded-frame cache.

## Lower preview buffering (2.0.16)

The Instant Replay VLC source now requests unbuffered video output in OBS.
VLC already schedules its display callbacks; OBS can present the newest delivered
picture without retaining an additional timed queue across seeks or slow-motion
frame updates. This applies to existing VLC replay sources after restarting OBS.
VLC caching and audio/video clock synchronization remain unchanged.

A local OBS pipeline probe compared decoded reference pictures after rapid seeks.
It measured lower queue latency with unbuffered output on the test clips, but
large or heavily compressed clips can still take time to decode. This does not
guarantee a particular seek time on every recording or computer.
