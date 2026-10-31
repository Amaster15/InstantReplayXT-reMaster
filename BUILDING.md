# Building and validating InstantReplayXT

For installing and using the plugin, see [SETUP.md](SETUP.md). These instructions
are for contributors. Versioned filenames below follow the version in `CMakeLists.txt`;
update all version fields together when preparing a release.

## Build a Windows x64 package

Prerequisites on Windows: Git, Python 3.12+, CMake 3.28+, and Visual Studio 2022 with
**Desktop development with C++**, the MSVC x64/x86 tools and a Windows SDK.
First build the trimming helper in an **MSYS2 UCRT64** terminal with `make`,
`mingw-w64-ucrt-x86_64-gcc` and `mingw-w64-ucrt-x86_64-python` installed:

```sh
python scripts/build_trimmer.py
```

Then run from a checkout/extracted source folder in the normal Windows terminal:

```powershell
python scripts/build.py
```

The script downloads the pinned official OBS 31.0.0 source, verifies its Git commit,
builds its SDK, then builds the plugin. OBS's bootstrap downloads and verifies its
2024-09-12 dependency bundles, including Qt. The first build requires internet access
and several GB of disk space. Output:

```text
artifacts/InstantReplayXT-2.0.22-obs31-windows-x64.zip
artifacts/InstantReplayXT-2.0.22-obs31-windows-x64.zip.sha256
```

Before packaging, the script loads the compiled module to verify native dependencies,
required entry points and its OBS 31 API version. This does not start replay playback.
The plugin, locale, updater, trimming helper/source, documentation and build metadata are packaged. OBS/Qt runtime
DLLs are not bundled or replaced. The installed OBS application provides those runtimes.
The GitHub Actions Windows job runs the same script. The initial Windows and Linux packages were built successfully in
[GitHub Actions run 1](https://github.com/Amaster15/InstantReplayXT-reMaster/actions/runs/40807018374).
Use the workflow run for the revision you are building; the historical run is not a current download.


## Build a Linux baseline package

Use Ubuntu 24.04 x86_64 with Qt 6.4. Install the dependencies listed in
[the workflow](https://github.com/Amaster15/InstantReplayXT-reMaster/blob/main/.github/workflows/build.yml), including Python 3.12+, GCC and make, then run:

```sh
xvfb-run -a python3 scripts/build.py --test
```

Use `python3 scripts/build.py --test` instead if a working X display is already available.
The script builds the actual OBS 31 SDK; it does not relabel a binary built against a newer SDK.
It also builds the pinned FFmpeg helper from SHA256-verified official source. The helper
uses a temporary build directory without spaces (override with `--work-dir` when
running `scripts/build_trimmer.py` directly); no external codecs or network protocols
are enabled. `ffprobe` is built for packet-level tests but is not shipped in installers.
It stages a `.tar.gz` and SHA256 checksum under `artifacts/`. Linux baseline packaging
rejects Qt 6.5+ to prevent accidental dependence on newer Qt symbols.
Ubuntu-built packages still depend on compatible distro C/C++/Qt libraries. Flatpak and
other sandboxed OBS distributions require builds for their matching runtime.

For a local development build using an installed OBS 31+ SDK and Qt 6.4+:

```sh
python3 scripts/build_trimmer.py
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release -DIRXT_BUILD_TESTS=ON
cmake --build build-local --parallel
ctest --test-dir build-local --output-on-failure
cmake --install build-local --prefix "$PWD/dist"
```

Use the CMake workflow above for development. Controller and module-lifecycle integration
tests run on Linux because their fake frontend relies on ELF symbol interposition.
The clip-trimming and player tests also run in Windows CI. A display or `xvfb-run`
is required for libobs tests; `QT_QPA_PLATFORM=offscreen` alone is not enough.


## Before release

Validate actual replay media, audio, transitions/stingers, Studio Mode, restart/hotkey
persistence, missing sources, Unicode backup folders, and exit/collection changes during
replay on Windows 10 + OBS 32 and Linux. Use a test scene collection. Repeat on OBS 31 to
establish the minimum runtime and OBS 33 to check forward compatibility.

The tests exercise real libobs source/data ownership and real Qt objects, but their fake
frontend and media source do not substitute for a full OBS playback test.


## Generate installers

After the matching platform package has been built:

```sh
python scripts/package_installers.py --platform windows
# Or on Linux:
python3 scripts/package_installers.py --platform linux
```

Windows installer generation requires Inno Setup 6. Artifacts and SHA256 files
are written under `artifacts/`. `python3 tests/test_installers.py` exercises the
Linux installer in temporary directories. Windows installer tests require a
disposable GitHub Actions runner; they must not be run against a personal install.

## Compatibility and validation limits

Release packages target the real OBS 31.0.0 SDK, Qt 6.4 on Ubuntu 24.04, and the
OBS 31 Windows dependency bundle. Local builds against newer OBS/Qt versions are
for development and must not be published as baseline installers. Every release must pass Windows and Linux CI, including installer checks, before publication. Live OBS testing on the target setup remains important. Future OBS major versions need separate validation.
