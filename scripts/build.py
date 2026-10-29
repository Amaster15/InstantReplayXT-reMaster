#!/usr/bin/env python3
"""Build the OBS 31 SDK, plugin, and package locally (Windows x64 or Linux)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OBS_COMMIT = "0b2c85845e20f41c612cfe2583e380bd436b6b3c"


def run(*args):
    print("+", subprocess.list2cmdline([str(a) for a in args]), flush=True)
    subprocess.run([str(a) for a in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 4))
    parser.add_argument("--test", action="store_true", help="Run Linux integration tests; needs a display or xvfb-run")
    args = parser.parse_args()
    system = platform.system()
    if system not in ("Windows", "Linux") or platform.machine().lower() not in ("amd64", "x86_64"):
        parser.error("Packaged builds currently support Windows x64 and Linux x86_64")
    if args.test and system != "Linux":
        parser.error("The frontend integration-test shim currently runs on Linux only")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    cmake = shutil.which(args.cmake)
    if not cmake:
        parser.error("Install CMake 3.28 or newer, or pass --cmake PATH")
    run(sys.executable, ROOT / "tests" / "test_update_helper.py")
    slug = system.lower() + "-x64"
    deps = ROOT / ".deps" / slug
    source = deps / "obs-studio-31.0.0"
    sdk_build = deps / "build-sdk"
    sdk = deps / "sdk"
    build = ROOT / ("build-" + slug)
    stage = build / "package"
    deps.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        run("git", "clone", "--depth", "1", "--branch", "31.0.0",
            "https://github.com/obsproject/obs-studio.git", source)
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if head != OBS_COMMIT:
        raise RuntimeError(f"Unexpected OBS source revision: {head}; expected {OBS_COMMIT}")
    if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain", "--untracked-files=no"], text=True).strip():
        raise RuntimeError("OBS source has tracked modifications; baseline builds require pristine source")
    generator = ["-G", "Visual Studio 17 2022", "-A", "x64"] if system == "Windows" else ["-G", "Ninja"]
    run(cmake, "-S", source, "-B", sdk_build, *generator,
        "-DCMAKE_BUILD_TYPE=Release", "-DENABLE_UI=OFF", "-DENABLE_PLUGINS=OFF",
        "-DENABLE_BROWSER=OFF", "-DENABLE_SCRIPTING=OFF", "-DENABLE_PULSEAUDIO=OFF",
        "-DENABLE_WAYLAND=OFF", "-DOBS_VERSION_OVERRIDE=31.0.0")
    run(cmake, "--build", sdk_build, "--config", "Release", "--target", "obs-frontend-api", "--parallel", args.jobs)
    run(cmake, "--install", sdk_build, "--config", "Release", "--component", "Development", "--prefix", sdk)
    # OBS's Windows bootstrap downloads and SHA256-verifies its pinned obs-deps/Qt.
    prefixes = [sdk]
    if system == "Windows":
        prefixes.extend(sorted((source / ".deps").glob("obs-deps-*-x64")))
    run(cmake, "-S", ROOT, "-B", build, *generator, "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_PREFIX_PATH=" + ";".join(str(p) for p in prefixes),
        "-DIRXT_REQUIRE_BASELINE=ON", "-DIRXT_PLUGIN_LAYOUT=ON", "-DIRXT_BUILD_TRANSPORT_TESTS=ON",
        "-DIRXT_BUILD_TESTS=" + ("ON" if args.test else "OFF"))
    run(cmake, "--build", build, "--config", "Release", "--parallel", args.jobs)
    # Test native library loading before publishing an archive. This resolves all
    # imports but deliberately does not call obs_module_load outside the OBS UI.
    module = build / "Release" / "InstantReplay.dll" if system == "Windows" else build / "InstantReplay.so"
    verify_args = []
    if system == "Windows":
        dll_dirs = {p.parent for prefix in prefixes for p in prefix.rglob("*.dll")}
        for directory in sorted(dll_dirs):
            verify_args.extend(["--dll-directory", str(directory)])
    run(sys.executable, ROOT / "scripts" / "verify_module.py", module, *verify_args)
    if system == "Windows":
        # Run with native Windows curl and no Qt TLS plugin search paths.
        test_env = os.environ.copy()
        test_env["PATH"] = os.pathsep.join(str(p) for p in sorted(dll_dirs)) + os.pathsep + test_env.get("PATH", "")
        subprocess.run([str(build / "Release" / "update-transport-test.exe"), "--live"],
                       check=True, env=test_env)
        subprocess.run([str(build / "Release" / "replay-player-test.exe"),
                        str(ROOT / "tests" / "fixtures" / "replay-test.mp4")], check=True, env=test_env)
    if args.test:
        ctest = Path(cmake).with_name("ctest.exe" if system == "Windows" else "ctest")
        run(ctest, "--test-dir", build, "--output-on-failure")
    if stage.exists():
        shutil.rmtree(stage)
    run(cmake, "--install", build, "--config", "Release", "--prefix", stage)
    provenance = {"obs_sdk": "31.0.0", "obs_commit": head, "platform": slug,
                  "native_dependency_and_export_check": "passed",
                  "live_obs_playback_validation": "Not performed by this build script"}
    provenance["build_versions"] = (build / "build-versions.txt").read_text(encoding="utf-8").splitlines()
    (stage / "build-info.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    artifacts = ROOT / "artifacts"
    artifacts.mkdir(exist_ok=True)
    name = "InstantReplayXT-2.0.10-obs31-" + slug
    if system == "Windows":
        archive = artifacts / (name + ".zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as out:
            for file in sorted(stage.rglob("*")):
                if file.is_file():
                    out.write(file, file.relative_to(stage))
    else:
        archive = artifacts / (name + ".tar.gz")
        with tarfile.open(archive, "w:gz") as out:
            for file in sorted(stage.iterdir()):
                out.add(file, arcname=file.name)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_name(archive.name + ".sha256").write_text(digest + "  " + archive.name + "\n", encoding="ascii")
    print("Package:", archive)


if __name__ == "__main__":
    main()
