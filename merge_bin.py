#!/usr/bin/env python3
"""
merge_bin.py - PlatformIO post-build script.

Boards like the ESP32-C3-DevKitM-1 build three pieces:
  * bootloader.bin  (offset 0x0000)
  * partitions.bin  (offset 0x8000)
  * firmware.bin    (offset 0x10000)

This script merges them into a single "dist/firmware.bin" that the user
can flash with one esptool.py command:

    python -m esptool --chip esp32c3 -p COMx write_flash 0x0 dist/firmware.bin
"""

import os
import sys
import shutil

# SCons / PlatformIO passes us the build environment as the second CLI
# argument (when invoked via `extra_scripts = post:merge_bin.py`).
Import = None
try:
    Import("env")  # noqa: F821 - injected by PlatformIO
except NameError:
    Import = None  # type: ignore

# We may also be invoked from the command line for debugging.
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))


def _build_dir():
    if Import is not None:
        return os.path.join(str(Import("PROJECT_BUILD_DIR")),
                            Import("PIOENV"))  # type: ignore
    # Fall back to the conventional PlatformIO path.
    return os.path.join(PROJECT_DIR, ".pio", "build", "esp32c3")


def _dist_dir():
    return os.path.join(PROJECT_DIR, "dist")


def _safe_load(bin_path, label):
    with open(bin_path, "rb") as fp:
        data = fp.read()
    if not data:
        sys.stderr.write("[merge_bin] %s is empty: %s\n" % (label, bin_path))
        sys.exit(1)
    return data


def main():
    build_dir = _build_dir()
    dist_dir = _dist_dir()
    os.makedirs(dist_dir, exist_ok=True)

    bootloader = _safe_load(os.path.join(build_dir, "bootloader.bin"), "bootloader")
    partitions = _safe_load(os.path.join(build_dir, "partitions.bin"), "partitions")
    app        = _safe_load(os.path.join(build_dir, "firmware.bin"),   "firmware")

    merged_path = os.path.join(dist_dir, "firmware.bin")
    with open(merged_path, "wb") as fp:
        # The firmware image is appended at the end of the bootloader's
        # 0x10000-byte slot.  The bootloader ships at the start of flash.
        fp.write(bootloader)
        # Pad to 0x8000 (partitions slot).
        if len(bootloader) > 0x8000:
            sys.stderr.write("[merge_bin] bootloader larger than 0x8000!\n")
            sys.exit(2)
        fp.write(b"\xff" * (0x8000 - len(bootloader)))
        fp.write(partitions)
        # Pad to 0x10000 (application slot).
        fp.write(b"\xff" * (0x8000 - len(partitions)))
        fp.write(app)

    size_kb = os.path.getsize(merged_path) / 1024.0
    print("[merge_bin] wrote %s (%.1f KiB)" % (merged_path, size_kb))
    return 0


if __name__ == "__main__":
    sys.exit(main())
else:
    # PlatformIO calls us as a SCons extension.  The "env" object is the
    # build environment; we don't need anything from it.
    main()
