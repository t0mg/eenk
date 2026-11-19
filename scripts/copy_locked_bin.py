#!/usr/bin/env python3
"""
scripts/copy_locked_bin.py — PlatformIO post-build artifact rename
==================================================================
Post-build action for [env:esp32c3_locked] (see platformio.ini).

Copies the app-only ``firmware.bin`` to ``eenk-x4-locked.bin`` so the
SD-flashable artifact has a distinct, releasable name that can never be
confused with ``firmware-factory.bin`` (USB-only merged image) or the stock
``firmware.bin``. Byte-identical copy; the original is left in place.
"""

import os
import shutil

Import("env")  # noqa: F821  (PlatformIO injects this into the SCons context)


def copy_locked_bin(source, target, env):
    build_dir = env.subst("${BUILD_DIR}")
    src = os.path.join(build_dir, "firmware.bin")
    dst = os.path.join(build_dir, "eenk-x4-locked.bin")
    if os.path.exists(src):
        shutil.copyfile(src, dst)
        print(f"Copied locked artifact: {src} -> {dst}")
    else:
        print(f"Warning: {src} not found, skipping locked-bin copy")


env.AddPostAction("buildprog", copy_locked_bin)
