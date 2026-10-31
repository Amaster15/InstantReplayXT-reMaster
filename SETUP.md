# Setup and daily use

[Back to the project overview](README.md)

Follow sections 1–5 for your first replay. Sections 6–9 cover optional match folders,
clip playlists, playback controls, transferring settings, and updates.

**Version note:** the three clip-length hotkeys and grey/Active match-row styling
are new in **2.0.21**. They are not available in v2.0.20. Playlist controls are named: **Matches / Playlist** and **Playlist VLC source**
were labeled **Matches / BRB playlist** and **BRB VLC source** in older releases.
This guide covers 2.0.22, including optional scene return and audio-source mute selection.

## 1. Install the plugin

You need OBS Studio **31 or newer**, a supported 64-bit operating system, and a
writable folder with room for your clips. Download installers from this project's
[GitHub Releases](https://github.com/Amaster15/InstantReplayXT-reMaster/releases/latest).
Choose a platform installer under **Assets**, not GitHub's **Source code** archive.

For the default **VLC live slow motion** player, install **VLC 3.x** from
[VideoLAN](https://www.videolan.org/vlc/) or your Linux distribution. Windows needs
**64-bit VLC**, normally installed in `C:\Program Files\VideoLAN\VLC`. Linux needs
VLC/libVLC 3.x with its codec plugins. Restart OBS after installing VLC. VLC 4 is
not supported by this custom player. VLC is not included with the plugin installer.
If you choose **Standard Media Source** instead, VLC is not needed for replay
playback; speed changes will reload the clip and reverse is unavailable.

### Windows installation

1. Finish any recording or stream and close all OBS instances.
2. Run the release file ending in **`-windows-x64-setup.exe`**.
3. Accept the administrator prompt and keep the default plugin destination:

   ```text
   C:\ProgramData\obs-studio\plugins\InstantReplay
   ```

4. Finish installation, restart OBS, and check for **Tools > Instant Replay XT Settings**.

The destination chooser installs the **plugin**, not your video clips. Use a custom
folder only if OBS is configured to search its parent, and keep the final folder
named `InstantReplay`. An arbitrary folder will not make the plugin appear in OBS.
Uninstall the old location before moving to a different one to avoid duplicate copies.

If setup reports an old `InstantReplay.dll` in OBS's application folder, close OBS
and move that old DLL out of `obs-plugins\64bit` before trying again. Keep only one
installed copy. The installer backs up the previous DLL under
`%ProgramData%\obs-studio\plugin-backups`. It is unsigned, so Windows may identify
its publisher as unknown.

### Linux installation

Use the release file ending in **`-linux-x64-install.run`** with a compatible
**native OBS installation**. The baseline is Ubuntu 24.04 / Qt 6.4; other distributions
need compatible runtime libraries. This installer is **not for Flatpak OBS**.

1. Install Python 3 and `pgrep` (`procps` on Ubuntu) if they are missing.
2. Close OBS.
3. Open a terminal in the download folder and run the installer as your normal
   desktop user, **without sudo**. Replace `VERSION` with the downloaded version:

   ```sh
   sh "InstantReplayXT-VERSION-obs31-linux-x64-install.run"
   ```

4. Restart OBS and check for **Tools > Instant Replay XT Settings**.

The default destination is
`${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/InstantReplay`.
An upgrade backs up the existing plugin under OBS's `plugin-backups` directory.

Use `--dry-run` to display the destination without installing. To choose a custom
folder, add `--install-dir "/absolute/OBS-plugin-search-folder/InstantReplay"`.
That parent folder must already be in OBS's plugin search paths. Uninstall the old
copy before relocating, and use the same `--install-dir` for later upgrades/uninstall.

### Portable Windows OBS

Use the Windows ZIP package instead of the standard installer. With OBS closed:

| File or folder inside the ZIP | Destination under your portable OBS folder |
| --- | --- |
| `InstantReplay/bin/64bit/InstantReplay.dll` | `obs-plugins/64bit/InstantReplay.dll` |
| Everything inside `InstantReplay/data/` | `data/obs-plugins/InstantReplay/` |

Copy the **whole data folder**, including locale, updater files, and the trimming
helper when provided. Copying only the DLL and locale leaves features missing.
Use manual release downloads for future updates; the built-in installer updater
does not support this portable layout.

## 2. Set up OBS and the replay scene

1. Open **OBS Settings > Output** and choose your recording path, format, encoder,
   and audio tracks. For a first test, H.264 video with AAC audio in MKV is a useful
   starting point. OBS explains the settings in its
   [recording output guide](https://obsproject.com/kb/standard-recording-output-guide).
2. Enable **Replay Buffer**. In Advanced output mode it has its own **Replay Buffer**
   tab. Set **Maximum Replay Time** to the longest clip you want to keep: for the
   upcoming 15/30/60-second presets, use at least **60 seconds**. Make sure the buffer's
   memory allowance can hold that much history at your recording quality.
3. If your recording encoder exposes **Keyframe Interval**, a 1–2 second interval
   helps keep fast-trim lengths closer to the requested length. The buffer captures
   your OBS program output; it cannot recover moments from before it started.
4. Create a separate OBS scene, for example **Replay Scene**.
5. Inside it, add **Sources > + > Media Source** and name it, for example,
   **Instant Replay Media Source**. Keep it directly in the scene, **outside groups**.
   A sample video can help you size and position it; the plugin will replace its
   file when you save or select a replay. Leave this source dedicated to replays.
6. Return to your live/game scene and click **Start Replay Buffer** in OBS's Controls.

You do **not** need to replace the Media Source with OBS's regular VLC Video Source.
The plugin creates its own **Instant Replay VLC Player** for live slow motion.
A regular **VLC Video Source** is used separately for the optional clip playlist.

## 3. Configure Instant Replay XT

Open **Tools > Instant Replay XT Settings > Replay settings**:

1. Check **Enable**.
2. Choose **Replay Scene** from the scene dropdown.
3. Choose the standard **Media Source** you just created from **Replay Source Name**.
   Do not choose the generated source ending in `(VLC live)`. The list shows standard
   Media Sources directly inside the selected scene and refreshes when opened.
4. Use **Replay folder > Browse…** to choose where the current replay is saved.
   Leave it blank to use the folder containing OBS's saved replay.
5. To preserve older clips, check **Backup Replay File**, then use **Backup File Path >
   Browse…** to choose a separate backup folder. Match folders can replace this
   single folder later. Without backups, each new save replaces the previous replay.
6. Leave **Automatically switch to replay scene after saving** off if you want to
   decide when to show a replay. Turn it on if saving should also play the replay.
7. Open **Clip library** and keep **Replay player > VLC live slow motion** selected.
   On playback, or when you select the player mode, the plugin creates a source
   ending in **`(VLC live)`**, copies the standard source's initial placement and
   audio routing, and uses it in the replay scene. The standard source stays there
   for switching back. Ensure the chosen player's source is visible in that scene.

### Timing and audio settings

| Setting | Meaning and starting point |
| --- | --- |
| **Write Pause (s)** | Maximum wait for OBS to report a saved replay. Start with 10 seconds. Zero still waits at least one second. This is not the clip length. |
| **Transition Pause (ms)** | Transition allowance for replay scene changes. Start with 300 ms; adjust for your scene transition. |
| **Replay Length (s)** | Timing budget for automatic playback after saving, adjusted for playback speed. This does not change the saved video length. |
| **Mute Desktop Audio** | Temporarily mute global Desktop Audio channels while replaying. |
| **Mute Mic/Aux** | Temporarily mute global Mic/Aux channels while replaying. Leave off if you want live commentary. |

The mute options do not cover arbitrary application-audio sources. Check the replay
source's OBS mixer level, output tracks, and monitoring settings for your stream.
The custom VLC player outputs stereo audio; saved clips retain their recorded tracks.

Changes save to a separate config file as you edit. Check the message along the
bottom of the window; **Save settings** also requests an OBS settings save.

## 4. Assign your hotkeys

Open **OBS Settings > Hotkeys**, search for **Instant Replay XT**, assign distinct
keys, and apply the changes. Example keys below are suggestions; avoid existing
shortcuts in OBS and your game.

| Plugin action | Example key | What it does |
| --- | --- | --- |
| **Instant Replay XT: Save replay** | Ctrl+Alt+F9 | Saves the full currently available buffer. |
| **Instant Replay XT: Play latest replay** | Ctrl+Alt+F10 | Plays the latest replay in the replay scene; optional return waits for the actual clip end. |
| **Instant Replay XT: Save clip preset 1** *(2.0.21)* | Ctrl+Alt+F1 | Saves approximately the latest 15 seconds by default. |
| **Instant Replay XT: Save clip preset 2** *(2.0.21)* | Ctrl+Alt+F2 | Saves approximately the latest 30 seconds by default. |
| **Instant Replay XT: Save clip preset 3** *(2.0.21)* | Ctrl+Alt+F3 | Saves approximately the latest 60 seconds by default. |

Do not bind the same key to OBS's own **Save Replay** action. The plugin requests
that save itself. Avoid other replay-save requests while the plugin is saving.

### Configurable clip lengths — 2.0.21

Open **Clip hotkeys** to change the three preset lengths (1–3600 seconds). The
hotkey names stay numbered even if you change their lengths. Your lengths autosave
and are included in config exports; key assignments are managed by OBS.

Fast saving copies compressed video and all audio tracks without re-encoding. It
starts at the preceding keyframe, so a clip may be longer by up to a keyframe
interval. If OBS has less history, only that available history can be saved.
The plugin does not resize or restart the running buffer. Fast seeking behavior is
also described in the [FFmpeg documentation](https://ffmpeg.org/ffmpeg.html#Main-options).

The matching release installer includes the trimming helper; you do not need a
separate FFmpeg install. The helper supports MKV, MP4, MOV, TS, FLV and AVI. If it
cannot trim reliably, the status reports the failure and OBS's original is kept.
The fixed replay and numbered backup both receive the shortened clip.

## 5. Test your first replay

1. Stay on your live/game scene with **Replay Buffer running**. Let it collect
   enough video; wait at least 60 seconds to test a full 60-second preset.
2. Press the plugin's **Save replay** hotkey (or a preset hotkey once available).
3. Wait for the plugin status to show **Main replay saved at…**. Check that folder
   for `InstantReplay.mkv`, or the extension matching your recording format.
4. If backups are enabled, check for `InstantReplay_1.mkv` in the selected backup
   folder. Save again: the main file should be replaced and the next numbered
   backup should appear. Existing numbered files are not overwritten.
5. In **Clip library**, click **Refresh clips**, then the **screen-with-play icon**
   beside the clip. OBS should switch to your replay scene, play it, and return
   to the previous scene at the clip's end if **Return to previous scene when the clip ends** is checked.
6. Verify picture and sound in a short test recording. Try **50%**, **Pause**, and
   **Resume**. Press **100%** to return to normal speed.
7. Close OBS normally, reopen it, and check that your selected folders, player,
   scene/source choices, and hotkeys remain set. Start Replay Buffer again before
   saving new clips.

If a step fails, use [Troubleshooting](#10-troubleshooting) before using the setup live.

## 6. Match folders and clip playlists

### Organize match backups

1. Keep **Backup Replay File** enabled on **Replay settings**.
2. Open **Matches / Playlist** and check **Use match folders instead of the single
   backup folder**.
3. Click **Add folder…** for each match, selecting or creating a separate folder.
   Edit **Match / teams** to a useful name, such as `Match 1 — Tigers vs Wolves`.
4. Use **Up** and **Down** to put the matches in order.
5. Select your first enabled match and click **Use selected match**. Confirm the
   **Active:** label below the list shows the correct folder.
6. Save clips throughout that match. The folder stays active until you change it.
7. Click **Next match** when ready. It skips unchecked rows and does not wrap at
   the end. Once there is no next match, match backups pause; the main replay
   still saves. Select an enabled match explicitly or add a new folder to resume.

**2.0.21 visual indicators:** the active row is bold and marked **Active**;
unchecked rows are grey/italic and marked **Off**; other usable rows show **Ready**.
Next match moves the row highlight to the new active folder. It does not uncheck
earlier matches automatically. Uncheck a row to disable it; check it again to make
it available. Disabled folders remain in the Clip library so their clips can still
be replayed.

Each folder continues its own backup numbering. **Remove from list** removes only
its configuration entry, not files on disk. The library stops scanning a removed
folder unless another configured entry points to it.

### Play clips in any scene

1. In the scene where you want clips to play, create a dedicated OBS **VLC Video Source**.
   Give it a descriptive name, such as **Clip Playlist**. Set its loop/shuffle options as desired.
2. In **Matches / Playlist**, select it from the **Playlist VLC source** dropdown.
3. Click **Refresh VLC now (restarts playlist)**. This replaces that source's
   playlist with the **active match folder only**. If match mode is off, it uses
   the single backup folder instead. Existing loop/shuffle settings are kept.
4. Optionally check **Refresh VLC after backups and match changes**. Automatic
   refresh waits while that source is visible, including in Studio Mode preview.
   It runs once the source is off-screen. Manual refresh runs immediately and
   restarts the playlist, so choose when to use it during a stream.

Use a dedicated VLC source: refreshing replaces its playlist. If match mode has
no active folder, refreshing supplies an empty playlist. A nonexistent folder
produces an error and leaves the current playlist unchanged.

## 7. Clip library and playback controls

The library lists the current replay plus files in the single backup folder and
all configured match folders, including disabled folders and subfolders. It scans
while the tab is open; **Refresh clips** scans immediately. Clips are newest first.
It cannot recover a main replay that was overwritten without a backup.

| Control | Behavior |
| --- | --- |
| **Play triangle** beside a clip | Loads that clip without changing scenes. If the replay scene is already showing, it plays there; otherwise enter the scene manually. |
| **Screen with play triangle** | Switches to the replay scene and plays that clip. The return checkbox controls whether it returns at the end. |
| **Play selected clip** / double-click | Follows **Switch scene for Play selected clip / double-click**. The two row icons always use their own behavior. |
| **Playback speed** | 10–200%; quick buttons are 10%, 25%, 50%, 75%, and 100%. Changes playback, not the saved file or clip playlist. |
| **Pause / Resume** | Freeze the current frame, then continue. Pausing also holds a timed scene return. |
| **Clip position** | Scrub backward or forward. Paused clips stay paused; playing clips keep playing. |
| **Reverse / Forward** | VLC only. Reverse uses silent backward seeks at the selected target speed, pauses at the beginning, and can be choppy or slower when decoding cannot keep up. |

Clicking another clip replaces the current playback; clicking the same clip
restarts it. File saving must finish before playback can replace that file.
A new clip starts in forward direction. Manually leaving the replay scene cancels
its automatic return.

**Return to previous scene when the clip ends** (2.0.22) is an optional checkbox in Clip library, off by default. It applies to Play + switch, Play selected/double-click with scene switching enabled, and the Play latest replay button/hotkey. The previous scene is remembered when playback switches scenes. Replacing the clip keeps that return scene.

Return happens only when forward playback reaches the actual clip end. Pause, reverse, reaching the first frame, playback errors, and stopping do not trigger a return. Returning uses your OBS scene transition. Manually leaving the replay scene cancels the pending return; manually entering it does not arm a return. The checkbox is saved and included in config export/import; its value is applied when you start playback.

**Replay Length is different from file length.** Automatic playback after saving still uses the configured Replay Length timer. Button/hotkey and library playback follow the actual clip end.

VLC changes speed without reopening the clip. Standard Media Source reloads the
clip and attempts to restore position. Reverse and seeking are not guaranteed
frame-accurate or as smooth as normal forward playback, especially with long
keyframe intervals or high-resolution recordings.

## 8. Keep or transfer your settings

The bottom-row **Save settings**, **Export config…**, and **Import config…** buttons
are available from every tab.

- Settings autosave by scene-collection name to `InstantReplay/settings.json` in
  OBS's plugin configuration directory. Atomic writes keep the previous valid
  document as `settings.json.bak`. A failed write appears in the bottom status.
- Hover over that status to see the exact config path. Standard Windows OBS uses
  `%APPDATA%\obs-studio\plugin_config\InstantReplay\settings.json`; native Linux
  normally uses `~/.config/obs-studio/plugin_config/InstantReplay/settings.json`.
  Portable/XDG configurations can use different paths.
- **Export config…** writes the current collection's plugin settings to a JSON file.
- On another computer, install the plugin and VLC, create the OBS scene and Media
  Source, open the intended scene collection, then **Import config…** and confirm
  replacement. Current settings are backed up, and replay playback stops.
- Review scene/source dropdowns and every folder path after import. Windows paths
  do not automatically become Linux paths, and the files themselves are not copied.

Exports include replay settings, player/speed choices, match order and enabled
states, active match, playlist settings, and the upcoming preset lengths. They do **not**
include clips, OBS scenes/sources, or hotkey bindings. Transfer clips separately;
use OBS's scene-collection export/import for OBS-managed setup, or recreate it and
assign your hotkeys again. Autosaving plugin settings does not make video clips
or unsaved OBS changes recoverable after every crash.

## 9. Update or uninstall

### Update

1. Open **Updates > Check for updates**.
2. If an update is offered, click **Download and install after OBS closes**.
3. Wait for verification and the queued-installation message. Finish your stream
   or recording, then close all OBS instances normally within 30 minutes.
4. Complete setup (Windows may ask for administrator permission), then restart OBS.

Updates are checked/downloaded only when requested. The updater supports the
standard `InstantReplay/bin/64bit` install layout, including a custom folder chosen
by the installer. For portable/manual layouts, download the release and update
manually with OBS closed. **Cancel update** cancels a download or queued installation
while OBS remains open. **Open update downloads / log** shows diagnostics.

If the updater fails, download the matching installer from GitHub Releases and
install it with OBS closed. Do not mix files from different releases.

### Uninstall

- **Windows:** close OBS and use **Settings > Apps > Instant Replay XT for OBS**.
- **Linux:** close OBS and run the downloaded installer again with `--uninstall`,
  without sudo. Include the same `--install-dir` if you installed to a custom folder.

Uninstall preserves OBS settings, replay recordings, backups, and unrecognized
files. Previous plugin backups are separate from your video backup folders.

## 10. Troubleshooting

| Symptom | Check |
| --- | --- |
| No Instant Replay entry in Tools | Restart OBS. Check the correct x64 package, install location, compatible native runtime, and absence of an old duplicate DLL. A custom folder must be searched by OBS. |
| Pressing Save does nothing | Enable the plugin, start Replay Buffer, allow it to fill, and check the assigned key for conflicts with OBS's own Save Replay. Read the plugin status and verify folder permissions/free space. |
| Clip hotkeys tab is missing | These controls require 2.0.21; v2.0.20 does not contain them. Install a release that includes clip presets. |
| Preset clip is longer/shorter than expected | Fast trim starts at a keyframe. Check encoder keyframe spacing, buffer maximum time, memory allowance, and how long the buffer has been running. Replay Length does not determine saved length. |
| Timestamped OBS filename remains | Read the save error. The plugin preserves the original when trimming or publishing fails. Another player can lock the fixed file, or the destination may not be writable. |
| Trimming helper is missing | Reinstall the matching complete release. Manual installs must include the whole data folder. The full-buffer Save replay action is still available. |
| Backups are missing | Enable Backup Replay File. Check the single folder, or enable match mode and choose an active enabled match with a path. At the end of the match list, backups pause. |
| VLC selection fails or returns to the old choice | Install 64-bit VLC 3.x on Windows or VLC/libVLC 3.x on Linux and restart OBS. Choose a valid replay scene and a standard Media Source directly in that scene. Check the displayed error. |
| Black replay or no audio | Save a clip first; verify the selected scene/source and chosen player's visibility. Check OBS mixer levels, audio track routing, and source placement. Use the plugin's Play action after selecting the player. |
| Standard player restarts on speed change | This is expected. Choose VLC live slow motion for live speed control. |
| Rewind or scrubbing lags | Try lower-resolution clips or shorter recording keyframe intervals. Reverse is seek-based; it cannot guarantee smooth real-time reverse decoding. |
| Clip playlist did not change | Check the selected regular VLC Video Source and active folder. Automatic refresh waits while visible, including preview. Manual refresh restarts immediately. |
| Settings revert after restart | Look for a config-write failure at the bottom, hover for its path, and confirm the same scene collection is loaded. Save settings, then export a backup. |
| Update check shows TLS initialization failed on v2.0.6/2.0.7 | Close OBS and manually install a current Windows release once; those older versions used a different HTTPS path. |
| Installer/update says OBS is running | Close every OBS instance before continuing. |

For a reproducible issue, open **OBS Help > Log Files > View Current Log** or
**Show Log Files**. Include the full log from the affected session, your plugin/OBS
versions, operating system, player mode, exact error, and steps that caused it.
Plugin messages contain **`[InstantReplayXT]`**. Submit the details through the
[project issue tracker](https://github.com/Amaster15/InstantReplayXT-reMaster/issues).

### Selecting audio to mute during replay (2.0.22)

Beside **Mute Desktop** and **Mute Mic/Aux**, choose the exact OBS audio source
from the dropdown and enable its checkbox. Both lists include audio-capable
sources from OBS, including scene-specific audio sources. Open a list to refresh
it after adding sources. The selection is saved and included in config exports.

The **Automatic** choice preserves the original behavior: desktop channels 1–2
or mic/aux channels 3–5. A named selection mutes only that source. If a named
source is unavailable, the plugin warns in replay status instead of choosing a
different source. After importing a config on another computer, select matching
sources there.

Replay status lists the sources muted when playback starts. OBS's Audio Mixer
also shows their mute state. Sources stay muted during pause/reverse and regain
their previous mute state when playback finishes or is cancelled. A source that
was already muted stays muted; selecting the same source twice restores it once.
OBS source muting affects that source everywhere it is used, so avoid choosing
the replay's own audio source unless you want a silent replay.
