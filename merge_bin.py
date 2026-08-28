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

Important: PlatformIO invokes `post:` extra_scripts after EACH sub-target
(bootloader, partitions, app, ...) is built.  We must therefore bail
out quietly if the three inputs are not all present yet, and only
produce the merged image on the final invocation.
"""

import os
import sys


def _build_dir():
    build_dir = os.environ.get('PROJECT_BUILD_DIR')
    pioenv = os.environ.get('PIOENV', 'esp32c3')
    if build_dir:
        return os.path.join(build_dir, pioenv)
    return os.path.join(os.getcwd(), '.pio', 'build', pioenv)


def _dist_dir():
    return os.path.join(os.getcwd(), 'dist')


def _read_all(bin_path):
    with open(bin_path, "rb") as fp:
        return fp.read()


def main():
    build_dir = _build_dir()
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partitions_path = os.path.join(build_dir, "partitions.bin")
    firmware_path   = os.path.join(build_dir, "firmware.bin")

    # Bail out quietly when called after a sub-target.  Only the call
    # that finds all three files does the actual merge.
    if not (os.path.isfile(bootloader_path) and
            os.path.isfile(partitions_path) and
            os.path.isfile(firmware_path)):
        return 0

    bootloader = _read_all(bootloader_path)
    if len(bootloader) > 0x8000:
        sys.stderr.write("[merge_bin] bootloader larger than 0x8000!\n")
        return 2
    partitions = _read_all(partitions_path)
    if len(partitions) > 0x8000:
        sys.stderr.write("[merge_bin] partitions larger than 0x8000!\n")
        return 2
    app = _read_all(firmware_path)

    dist_dir = _dist_dir()
    os.makedirs(dist_dir, exist_ok=True)
    merged_path = os.path.join(dist_dir, "firmware.bin")
    with open(merged_path, "wb") as fp:
        fp.write(bootloader)
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
    # PlatformIO may also import this file as a SCons post-script.
    main()
