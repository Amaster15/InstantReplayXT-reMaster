#!/usr/bin/env python3
"""Check native module dependencies, entry points and OBS API version (no UI)."""
import argparse
import ctypes
import os
from pathlib import Path


def verify(module, dll_directories=()):
    handles = []
    try:
        if os.name == "nt":
            for directory in sorted(set(Path(p).resolve() for p in dll_directories)):
                if directory.is_dir():
                    handles.append(os.add_dll_directory(str(directory)))
        library = ctypes.CDLL(str(Path(module).resolve()))
        for entry in ("obs_module_load", "obs_module_unload", "obs_module_set_pointer",
                      "obs_module_set_locale", "obs_module_free_locale", "obs_module_name"):
            getattr(library, entry)
        library.obs_module_ver.restype = ctypes.c_uint32
        version = library.obs_module_ver()
        if version != 31 << 24:
            raise RuntimeError(f"Module API version is {version:#x}, expected OBS 31.0.0")
        library.obs_module_name.restype = ctypes.c_char_p
        if library.obs_module_name() != b"Instant Replay XT":
            raise RuntimeError("Unexpected module name")
        print("Native dependency load, required exports, and OBS 31.0.0 API version verified.")
    finally:
        for handle in handles:
            handle.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("--dll-directory", action="append", default=[])
    args = parser.parse_args()
    verify(args.module, args.dll_directory)
