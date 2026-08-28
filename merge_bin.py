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

PlatformIO runs this as a "post:" extra_script.  The build directory and
environment name are exposed via env vars (PROJECT_BUILD_DIR, PIOENV)
that PlatformIO sets automatically, so we do NOT rely on `__file__`
(which is undefined inside a SCons exec context).
"""

import os
import sys


def _build_dir():
    build_dir = os.environ.get('PROJECT_BUILD_DIR')
    pioenv = os.environ.get('PIOENV', 'esp32c3')
    if build_dir:
        return os.path.join(build_dir, pioenv)
    # Fallback for CLI mode: assume CWD is the project root.
    return os.path.join(os.getcwd(), '.pio', 'build', pioenv)


def _dist_dir():
    # CWD is the project directory when invoked by PlatformIO.
    return os.path.join(os.getcwd(), 'dist')


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
        fp.write(bootloader)
        if len(bootloader) > 0x8000:
            sys.stderr.write("[merge_bin] bootloader larger than 0x8000!\n")
            sys.exit(2)
        fp.write(b"\xff" * (0x8000 - len(bootloader)))
        fp.write(partitions)
        fp.write(b"\xff" * (0x8000 - len(partitions)))
        fp.write(app)

    size_kb = os.path.getsize(merged_path) / 1024.0
    print("[merge_bin] wrote %s (%.1f KiB)" % (merged_path, size_kb))
    return 0


if __name__ == "__main__":
    sys.exit(main())
else:
    # PlatformIO may import this file as a SCons post-script.  Run
    # main() unconditionally so the merged image is always produced.
    main()
