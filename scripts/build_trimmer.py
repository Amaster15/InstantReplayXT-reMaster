#!/usr/bin/env python3
"""Build a small, file-only FFmpeg for lossless replay trimming (Linux or MSYS2)."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = "8.1.3"
SHA256 = "7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3"
URL = f"https://ffmpeg.org/releases/ffmpeg-{VERSION}.tar.xz"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="Use an already downloaded, checksum-verified source archive")
    parser.add_argument("--work-dir", type=Path, help="Build directory without spaces")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 4))
    args = parser.parse_args()
    windows = os.name == "nt" or os.environ.get("MSYSTEM") is not None
    slug = "windows-x64" if windows else "linux-x64"
    work = args.work_dir or Path(tempfile.gettempdir()) / ("irxt-trimmer-" + slug + "-" + hashlib.sha256(str(ROOT).encode()).hexdigest()[:8])
    if any(c.isspace() for c in str(work)):
        parser.error("FFmpeg requires a build directory without spaces; pass --work-dir PATH")
    work.mkdir(parents=True, exist_ok=True)
    archive = args.archive or work / f"ffmpeg-{VERSION}.tar.xz"
    if not archive.exists():
        urllib.request.urlretrieve(URL, archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise RuntimeError("FFmpeg source checksum mismatch")
    source = work / f"ffmpeg-{VERSION}"
    if not source.exists():
        with tarfile.open(archive) as compressed:
            compressed.extractall(work, filter="data")
    build = work / "build"
    build.mkdir(exist_ok=True)
    flags = ["--disable-autodetect", "--disable-network", "--disable-everything",
             "--disable-x86asm", "--disable-doc", "--disable-debug", "--disable-shared",
             "--enable-static", "--enable-ffmpeg", "--disable-ffplay", "--enable-ffprobe",
             "--enable-protocol=file,pipe",
             "--enable-demuxer=matroska,mov,mpegts,flv,avi",
             "--enable-muxer=matroska,mp4,mov,mpegts,flv,avi",
             "--enable-parser=h264,hevc,av1,vp8,vp9,aac,opus,vorbis,flac,mpegaudio,ac3",
             "--enable-decoder=h264,hevc,av1,vp8,vp9,aac,opus,vorbis,flac,mp3,ac3,pcm_s16le,pcm_s24le,pcm_f32le",
             "--enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,aac_adtstoasc,av1_frame_merge,vp9_superframe"]
    if windows:
        flags.extend(["--target-os=mingw32", "--extra-ldflags=-static"])
    subprocess.run(["bash", (source / "configure").as_posix(), *flags], cwd=build, check=True)
    subprocess.run(["make", f"-j{max(1, args.jobs)}", "ffmpeg" + (".exe" if windows else ""), "ffprobe" + (".exe" if windows else "")], cwd=build, check=True)
    stage = ROOT / "build-trimmer" / slug
    stage.mkdir(parents=True, exist_ok=True)
    executable = "ffmpeg.exe" if windows else "ffmpeg"
    shutil.copy2(build / executable, stage / executable)
    probe = "ffprobe.exe" if windows else "ffprobe"
    shutil.copy2(build / probe, stage / probe)
    subprocess.run([str(stage / executable), "-version"], check=True)
    # Ship the exact source, license and build recipe alongside the independent helper.
    shutil.copy2(archive, stage / f"ffmpeg-{VERSION}.tar.xz")
    shutil.copy2(source / "COPYING.LGPLv2.1", stage / "COPYING.LGPLv2.1")
    shutil.copy2(__file__, stage / "build_trimmer.py")
    (stage / "README.txt").write_text(
        f"FFmpeg {VERSION} — https://ffmpeg.org/\n"
        "Independent LGPL 2.1-or-later executable used for stream-copy trimming.\n"
        "No FFmpeg libraries are linked into InstantReplayXT.\n"
        "The complete original source archive, license and build recipe are included.\n"
        "No source changes. Build with Python 3.12+, bash, make and GCC (MSYS2 UCRT64 on Windows).\n"
        f"Source: {URL}\nSHA256: {SHA256}\n", encoding="utf-8")
    print("Trimmer package:", stage)


if __name__ == "__main__":
    main()
