# eenk - Interactive Fiction Firmware for ESP32 based Xteink ereaders

Welcome to **eenk**, a custom firmware designed to run **Ink** interactive fiction stories on the ESP32 based Xteink ereaders (X3, X4, X4 Pro), utilizing the [inkcpp](https://github.com/t0mg/inkcpp) C++ runtime.

> [!TIP]
> If you are interested in installing and using eenk rather than looking at firmware development, check out the [eenk website](https://t0mg.github.io/eenk/)!

This project supports four build configurations managed via PlatformIO:
- **`native`**: A desktop simulation of the runtime, using an 800x480 SDL2 graphical window to simulate the e-ink display for testing.
- **`esp32c3`**: The main firmware build for ESP32-C3 hardware (Xteink X3, X4).
- **`esp32c3_updater`**: A minimal recovery and OTA updater firmware for ESP32-C3 devices.
- **`esp32s3`**: The firmware build for ESP32-S3 hardware (Xteink X4 Pro).

---

## Project Architecture

The eenk ecosystem relies on two repositories working together through git submodules:

- **`eenk`** (the firmware, this repository): The core C++ firmware and SDL simulation runtime. It handles rendering to the e-ink display (or SDL window) and executing the compiled Ink binary using the `inkcpp` runtime. This is a PlatformIO project.
- **`eenky`** (the desktop IDE, [that repository](https://github.com/t0mg/eenky)): The Electron-based editor and toolchain. It provides the UI to write Ink stories, compile them to JSON and then to `.bin`, run the SDL simulator from `eenk`, and finally flash the compiled firmware to the device via Web Serial.

> [!NOTE]
> `eenky` includes `eenk` as a submodule to build the simulator backend.

---

## Desktop Simulation (`native` configuration)

The native configuration compiles a host executable that runs the compiled story interactively in an 800x480 SDL2 graphical window, to simulate the e-ink experience. This executable is used by the eenky IDE to preview stories before flashing them to the device.

### Building the Native Target

> [!NOTE]
> This has only been tested on Windows so far.

Ensure that the MinGW compiler suite (`g++`) is in your PATH. For example on Windows, you can install it via [MSYS2](https://www.msys2.org/), and the compiler path is typically `C:\msys64\mingw64\bin`.

```powershell
# Add MinGW bin folder to PATH and run PlatformIO build
$env:PATH="C:\msys64\mingw64\bin;$env:PATH"
pio run -e native
```

### Running the Native Target
Once built, the executable is created at `.pio/build/native/program.exe`. The runtime standard libraries and SDL2 are statically linked, so you can execute the program directly with a compiled story file:

```powershell
# Run with a compiled story
.pio\build\native\program.exe path/to/story.bin
```

Use the **Arrow Keys** to navigate choices, and **Enter** to confirm.

### Running Unit Tests
Unit tests are built using the PlatformIO Unity framework. The tests cover scripting detection, rendering components, and generate E-Ink simulator screenshots in `test/golden`.

```powershell
pio test -e native
```

---

## ESP32-C3 Target (`esp32c3` & `esp32c3_updater` configurations)

The `esp32c3` environment builds the firmware partition for physical ESP32-C3 hardware (Xteink X3, X4).

### Custom Partition Table & Updater Firmware

ESP32-C3 devices (16 MB flash) use a custom partition layout (`partitions.csv`):
- **`app0` (7 MB)**: Main interactive fiction runtime firmware (`esp32c3`).
- **`app1` (1 MB)**: Minimal OTA updater firmware (`esp32c3_updater`), which handles recovery and OTA updates.
- **`ink_cache` (~7.9 MB)**: Dedicated FAT data partition used for caching Ink story binaries.

> [!CAUTION]
> **USB-Locked Devices**: Installing `eenk` requires flashing custom bootloader and partition tables over USB. Because of this custom partition scheme, it is **strictly impossible to use `eenk` on USB-locked Xteink devices**, such as those sometimes sold on Aliexpress.

### Story Caching & Memory Mapping (`ink_cache`)

Because the ESP32-C3 lacks onboard PSRAM, its 400KB of internal SRAM is insufficient to keep full Ink story binaries in memory (in addition to the firmware itself). To address this:
1. The currently selected story binary is copied from the SD card to the `ink_cache` flash partition.
2. The `inkcpp` runtime utilizes ESP-IDF memory mapping (`esp_partition_mmap`) to map the partition memory directly into the MCU's address space.
3. This enables zero-copy execution of large Ink stories directly from flash without consuming scarce RAM. Up to ~5MB is possible, but the limit also largely depends on the number of Ink containers and variables, which require actual RAM.

### Building & Flashing

To compile the main firmware for the Xteink X4:
```powershell
pio run -e esp32c3
```
This produces `.pio/build/esp32c3/firmware.bin` (app partition) and **`.pio/build/esp32c3/firmware-factory.bin`** (merged bootloader + partition table + app, used by ESP Web Tools in the eenky IDE).

To compile the minimal updater firmware:
```powershell
pio run -e esp32c3_updater
```

Flashing via CLI:
```powershell
pio run -e esp32c3 -t upload
```

Alternatively, you can flash using the **Flash** tab within the eenky IDE, which uses ESP Web Tools to push `firmware-factory.bin` over USB.

---

## ESP32-S3 Target (`esp32s3` configuration)

The `esp32s3` environment builds the firmware for Xteink X4 Pro hardware.

### Key Architectural Differences

The ESP32-S3 target differs from the ESP32-C3 implementation in several key ways:

- **Symmetric A/B Partition Table**: Uses `partitions_x4pro.csv` with dual symmetric ~7.9 MB partitions (`app0` and `app1`). Full firmware updates are performed in-place by swapping active slots via standard ESP-IDF OTA APIs. A separate minimal updater firmware (`app1`) is not used.
- **Direct PSRAM Execution**: The X4 Pro includes onboard PSRAM, allowing Ink stories to be loaded directly into memory rather than using flash memory mapping via an `ink_cache` partition.
- **Hardware Enhancements**: Includes full support for **touchscreen input** and **LED indicator / backlight controls** present on X4 Pro hardware.

### Building & Flashing

To compile the firmware for the Xteink X4 Pro:
```powershell
pio run -e esp32s3
```

Flashing via CLI:
```powershell
pio run -e esp32s3 -t upload
```

---

## Releases & Versioning

### Version scheme

eenk uses [Semantic Versioning](https://semver.org/) with a `v` prefix, starting from `v0.1.0`.
- **Stable releases** - `v0.x.y` - pushed when a milestone is ready for general use.
- **Beta releases** - `v0.x.y-beta.N` - opt-in previews of the next point release.
- **Nightly builds** - auto-tagged `nightly-YYYYMMDD-<run>` - manual dispatch, no stability guarantee.

eenky (the IDE) uses the same scheme but is versioned **independently** - its `package.json` is the single source of truth.

### CI release channels

The GitHub Actions workflow ([`.github/workflows/deploy-docs.yml`](.github/workflows/deploy-docs.yml)) handles all firmware builds and creates GitHub Releases automatically.

| Trigger | Tag generated | `prerelease` flag | Web flasher |
|---------|--------------|-------------------|-------------|
| Push `v*` tag (e.g. `git tag v0.1.0 && git push --tags`) | The tag itself - `v0.1.0` | `false` | ✅ Shown as stable |
| `workflow_dispatch` with a tag supplied | The supplied tag | Configurable | Depends on flag |
| `workflow_dispatch` with no tag | `nightly-YYYYMMDD-<run>` | `true` | ⚠ Shown as pre-release only |

> [!NOTE]
> **All releases are manually triggered.** There are no automatic builds on commits to `main`. This keeps GitHub Releases clean during active development.

### Firmware version in device settings

Every build compiles `EENK_VERSION_STR` into flash via `scripts/build_version.py`, which runs `git describe --tags --always --dirty`. The string is surfaced in the device **Settings → Firmware** row.

| Build context | Example string |
|--------------|---------------|
| Clean tag build (CI) | `v0.1.0` |
| Local build, commits past tag | `v0.1.0-3-ga1b2c3` |
| Local build with uncommitted changes | `v0.1.0-3-ga1b2c3-dirty` |
| No tags in repo at all | `a1b2c3` (short SHA) |

---

## Credits & Acknowledgments

eenk builds upon the work of open-source firmware projects, drivers, and runtime libraries. See [credits.md](credits.md) for the complete list of credits and software acknowledgments.

## License

MIT; see [LICENSE](LICENSE).
