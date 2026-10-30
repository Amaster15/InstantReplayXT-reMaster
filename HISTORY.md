# Version history

These notes describe behavior when each version was released. Later versions replace
some of these behaviors (for example, the current minimum speed is 10%, not 1%).
For current instructions, use the [setup guide](SETUP.md).

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


## Preserve playback state while scrubbing (2.0.17)

The clip slider no longer automatically pauses and resumes the player. Playing
clips keep playing while seeking; paused clips remain paused. Releasing the
slider only submits the final seek, avoiding VLC's seek/resume race that could
jump playback back to an earlier position. Use Pause explicitly for freeze-frame
scrubbing.

Regression tests check decoded pictures after forward and backward seeks at
100%, 50% and 1%, and verify that the clip was not reopened. Panel tests verify
that dragging and releasing never toggle playback state.


## Reverse playback (2.0.18)

The clip library has a Reverse button next to Pause when using the VLC player.
It rewinds at the selected speed (1–200%); the same button changes to Forward
to return to normal direction. Speed presets and the speed slider also apply
while rewinding. Paused clips stay paused when changing direction; Resume
continues in the selected direction.

Reverse uses timed backward seeks with silent audio, not native negative-rate
decoding. It can look choppier than forward playback, especially with large clips
or widely spaced keyframes. It pauses at the beginning instead of looping.
Scrubbing during reverse establishes a new rewind position. Playing another
clip or restarting the current one restores forward direction.

Automatic scene return waits during rewind, then recalculates its remaining
time when returning to forward playback. Standard Media Source does not support
this reverse control; select the VLC player to enable it.



## Rewind decoding and 10% minimum (2.0.19)

The speed range is now 10–200%, with quick buttons for 10%, 25%, 50%, 75%,
and 100%. Saved speeds below 10% are raised to 10% when loaded.

Reverse waits for a newly decoded picture to be displayed before sending its
next seek. A slow decoder no longer builds a queue of obsolete seek requests,
and the rewind position no longer races ahead to zero before pictures arrive.
The first-frame pause is applied after the final seek produces a picture.
Seek previews decode at normal speed to avoid stretching VLC's buffering
at slow-motion rates. Playing seeks restore the selected speed after a picture
arrives; paused seeks restore it on Resume.

This is still silent, seek-based reverse. When decoding cannot keep up, rewind
slows down instead of skipping seconds of unseen footage. Smooth frame-by-frame
reverse at the selected rate is not guaranteed, especially for long keyframe
intervals and high-resolution footage.



## Portable config import/export (2.0.20)

Use **Export config…** next to Save settings to write a JSON file for the current
scene collection. On another computer, open the desired OBS scene collection
and choose **Import config…**. Import replaces that collection's plugin settings,
saves them immediately, and backs up the previous settings in the local config's
`.bak` file. Other collections are preserved. Import stops current replay playback
and is unavailable while a replay file is being saved or processed.

The file includes replay options, player mode, speed, scene/source names, backup
paths, ordered match folders with their enabled states and active match, and BRB
playlist settings. Clips, OBS scenes/sources, and OBS hotkey bindings are not
included. Copy video files separately and review folder paths and scene/source
names after importing, particularly when moving between Windows and Linux.
Invalid or unsupported export files are rejected before settings are replaced.

