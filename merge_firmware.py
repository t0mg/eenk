# merge_firmware.py
#
# This script is executed by PlatformIO as a post-build action.
# It uses esptool.py to merge the bootloader, partition table, boot app,
# and application binary into a single "factory" binary.
# This is necessary for web-based flashers like ESP Web Tools.

import os
import sys
from subprocess import run

Import("env")

def merge_bin(source, target, env):
    """
    Merges the built binaries into a single flashable file.
    """
    print("Running merge_bin.py post-build script...")
    
    # Get configuration from the build environment
    board_config = env.BoardConfig()
    chip = board_config.get("build.mcu", "esp32s3")
    flash_mode = board_config.get("build.flash_mode", "dio")
    flash_size = board_config.get("upload.flash_size", "16MB")
    flash_freq = env.subst("${__get_board_f_flash(__env__)}")

    # Define the output path for the merged binary
    merged_bin_path = os.path.join(env.get('BUILD_DIR'), "firmware-factory.bin")

    # The list of binaries to merge is provided by the build environment
    # in FLASH_EXTRA_IMAGES. This includes bootloader, partitions, and boot_app0.
    flash_images = [
        (item[0], env.subst(item[1])) for item in env.get("FLASH_EXTRA_IMAGES", [])
    ]
    
    # Add the main application binary
    app_offset = env.subst("$ESP32_APP_OFFSET")
    app_path = env.subst(target[0].get_abspath())
    flash_images.append((app_offset, app_path))

    esptool_path = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py"
    )

    # Define the command arguments for esptool.py
    # Using env.subst to let PlatformIO expand variables like $PYTHONEXE
    command = [
        env.subst('"$PYTHONEXE"'),
        f'"{esptool_path}"',
        "--chip", chip,
        "merge_bin",
        "--output", f'"{merged_bin_path}"',
        "--flash_mode", "dout", # flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
    ] + [item for sublist in flash_images for item in sublist]

    print("Merging binaries with command:")
    print(" ".join(command))

    # Execute the command using PlatformIO's environment
    ret = env.Execute(" ".join(command))

    if ret == 0:
        print(f"Successfully merged binaries to {merged_bin_path}")
    else:
        print(f"Error: Failed to merge binaries. Exit code: {ret}")
        env.Exit(1)

# Register the 'merge_bin' function as a post-build action for the firmware.bin target.
# This ensures the script runs automatically after a successful build.
env.AddPostAction("$BUILD_DIR/firmware.bin", merge_bin)
