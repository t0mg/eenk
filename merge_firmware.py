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
    merged_bin_path = env.subst("${BUILD_DIR}/firmware-factory.bin")

    # The list of binaries to merge is provided by the build environment
    # in FLASH_EXTRA_IMAGES. This includes bootloader, partitions, and boot_app0.
    flash_images = [
        (item[0], env.subst(item[1])) for item in env.get("FLASH_EXTRA_IMAGES", [])
    ]
    
    # Add the main application binary (app0)
    app_offset = env.subst("$ESP32_APP_OFFSET")
    app_path = env.subst("${BUILD_DIR}/${PROGNAME}.bin")
    flash_images.append((app_offset, app_path))

    # Add the updater application binary (app1) if it exists
    pio_env = env.get('PIOENV')
    updater_env = f"{pio_env}_updater"
    app1_path = os.path.join(env.get('PROJECT_DIR'), ".pio", "build", updater_env, "firmware.bin")
    if os.path.exists(app1_path):
        app1_offset = "0x800000" if "s3" in pio_env else "0x710000"
        print(f"Found app1 updater at {app1_path}, including it in factory binary at {app1_offset}")
        flash_images.append((app1_offset, app1_path))
    else:
        print(f"Note: app1 updater not found at {app1_path}. It will not be included in the factory binary.")

    esptool_path = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py"
    )

    command = [
        env.subst('"$PYTHONEXE"'),
        f'"{esptool_path}"',
        "--chip", chip,
        "merge_bin",
        "--output", f'"{merged_bin_path}"',
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size
    ]

    for offset, image in flash_images:
        command.extend([offset, f'"{image}"'])

    cmd_str = " ".join(command)
    print(f"Merging binaries with command:\n{cmd_str}")

    result = run(cmd_str, shell=True)
    if result.returncode != 0:
        print("Error: Failed to merge binaries.")
        env.Exit(1)
    else:
        print(f"Successfully merged binaries to {merged_bin_path}")

env.AddPostAction("buildprog", merge_bin)
