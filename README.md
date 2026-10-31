# Instant Replay XT — OBS 31+

Save instant replays, replay individual clips, control slow motion, and organize
match backups and clip playlists in OBS Studio. Targets **OBS 31 and newer**,
**Windows 10/11 x64**, and **native Linux x86_64**. OBS 32 on Windows 10 is the
primary user test setup. Future OBS major versions require compatibility checks.

## Start here

**[Read the complete setup guide](SETUP.md)** for installation, first-run setup,
a first-replay test, daily use, and troubleshooting.

- **[Download released installers](https://github.com/Amaster15/InstantReplayXT-reMaster/releases/latest)** — Windows `.exe` or Linux `-install.run`.
- **[Windows installation](SETUP.md#windows-installation)**
- **[Linux installation](SETUP.md#linux-installation)**
- **[OBS and VLC setup](SETUP.md#2-set-up-obs-and-the-replay-scene)**
- **[Save and test your first replay](SETUP.md#5-test-your-first-replay)**
- **[Match folders and clip playlists](SETUP.md#6-match-folders-and-clip-playlists)**
- **[Troubleshooting](SETUP.md#10-troubleshooting)**
- **[Build instructions](BUILDING.md)** · **[Version history](HISTORY.md)**

Version **2.0.22** adds optional return to the previous scene at the forward end of a clip and exact audio-source selectors for replay muting. Clip-length hotkeys and match-row styling were introduced in 2.0.21.

## Quick setup

1. Close OBS, install the matching release installer, and install **VLC 3.x** for
   live slow motion (64-bit VLC on Windows). Restart OBS.
2. Enable OBS Replay Buffer in **Settings > Output**, choose its duration and
   recording settings, and click **Start Replay Buffer**.
3. Create a replay scene with a regular **Media Source** directly inside it.
4. Open **Tools > Instant Replay XT Settings**, check **Enable**, choose that
   scene and Media Source in the dropdowns, and select your video save folders.
5. Keep **VLC live slow motion** selected in **Clip library**. The plugin creates
   its own VLC replay source; you do not need to turn the Media Source into a
   regular OBS VLC Video Source.
6. Assign the plugin's save and play hotkeys in **OBS Settings > Hotkeys**. Let
   the buffer collect video, save a replay, then play it from the Clip library.

See the setup guide for exact steps and expected results. The plugin installer
folder and replay video folders are separate settings.

## What it does

| Feature | Behavior |
| --- | --- |
| Save replay | Replaces `InstantReplay` plus the video extension; optional backups keep numbered copies. |
| Save clip presets (2.0.21) | Three configurable hotkeys, initially 15, 30 and 60 seconds, with fast trimming. |
| Play clips | Separate Play and Play + switch icons; optional automatic scene switching after saving. |
| Slow motion | 10–200% speed, quick presets, Pause/Resume, scrubbing, and VLC-only reverse. |
| Match folders | Stay on one match until Next match; disabled rows are skipped. |
| Clip playlist | Refresh a dedicated OBS VLC Video Source from the active match folder. |
| Settings | Autosaved per scene collection, with config export/import and a recovery backup. |
| Updates | Manually check GitHub, then queue installation after OBS closes. |

VLC 3.x is required for live slow motion and VLC playlist sources; VLC 4 is not supported
by the custom player. Standard Media Source remains available, but speed changes
reload its clip. Reverse uses backward seeks and may be slower or choppier than
forward playback. The Linux installer targets compatible native OBS installations,
not Flatpak. See the guide for these details.

## Project credits

This is a native C++/Qt 6 port of
[spreadfire/InstantReplayXT](https://github.com/spreadfire/InstantReplayXT), based on
commit `25807e749a3c4e8e1ad83790d323e3db3ff0718d`. Original authors: **A Docile Sloth
and spreadfire1**. Original copyright notices and the [GPL license](LICENSE) are
retained. This fork maintains the changes described here independently.

## Support development

If you find InstantReplayXT-reMaster useful, you can support its continued development:

<a href="https://buymeacoffee.com/amaster15">
  <img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&amp;emoji=&amp;slug=Amaster15&amp;button_colour=00ff00&amp;font_colour=000000&amp;font_family=Cookie&amp;outline_colour=000000&amp;coffee_colour=FFDD00" alt="Buy me a coffee" height="60">
</a>

Scan the QR code or click the button to support development:

<a href="https://buymeacoffee.com/amaster15">
  <img src="images/buy-me-a-coffee-qr.png" alt="Buy Me a Coffee QR code for Amaster15" width="240">
</a>

Donations are optional. Thank you for supporting the project!
