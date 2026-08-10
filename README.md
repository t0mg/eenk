# eenk - Interactive Fiction Firmware for Xteink X4

Welcome to **eenk**, a custom firmware designed to run **Ink** interactive fiction stories on the Xteink X4 e-ink device, utilizing the [inkcpp](https://github.com/t0mg/inkcpp) C++ runtime.

This project supports two primary build configurations managed via PlatformIO:
- **`native`**: A desktop simulation of the runtime, using an 800x480 SDL2 graphical window to simulate the e-ink display for testing.
- **`esp32c3`**: The firmware build for the actual ESP32-C3 microcontroller hardware.

---

## 1. Project Architecture

The eenk ecosystem relies on two repositories working together through git submodules:

- **`eenk` (This repository)**: The core C++ firmware and SDL simulation runtime. It handles rendering to the e-ink display (or SDL window) and executing the compiled Ink binary using the `inkcpp` runtime.
- **`eenky` (IDE)**: The Electron-based editor and toolchain. It provides the UI to write Ink stories, compile them to JSON and then to `.bin`, run the SDL simulator from `eenk`, and finally flash the compiled firmware to the device via Web Serial.

> [!NOTE]
> `eenky` is included as a submodule in `tools/eenky`. Conversely, `eenky` includes `eenk` as a submodule to build the simulator backend. They operate in tandem.

---

## 2. Desktop Simulation (`native` configuration)

The native configuration compiles a host executable that runs the compiled story interactively in an 800x480 SDL2 graphical window, rendering text with a bundled CP437 font to perfectly simulate the e-ink experience. This executable is used as the simulation backend for the eenky IDE.

### Building the Native Target
Ensure that the MinGW compiler suite (`g++`) is in your PATH. If you use MSYS2, the compiler path is typically `C:\msys64\mingw64\bin`.

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

Use `W/S` or the **Arrow Keys** to navigate choices, and **Enter** to confirm.

### Running Unit Tests
Unit tests are built using the PlatformIO Unity framework. The tests cover scripting detection, rendering components, and generate an E-Ink simulator screenshot (`test_battery_widget.pbm`).

```powershell
pio test -e native
```

---

## 3. ESP32-C3 Target (`esp32c3` configuration)

The `esp32c3` environment builds the firmware partition for the actual physical device.

### Building the Firmware
To compile the firmware for the Xteink X4, run:
```powershell
pio run -e esp32c3
```

This compiles the firmware and runs the `merge_firmware.py` post-build script to produce the merged binary ready for web flashing:
- `.pio/build/esp32c3/firmware.bin` (The app partition)
- **`.pio/build/esp32c3/firmware-factory.bin`** (The merged bootloader + partition table + app, used by ESP Web Tools in the eenky IDE)

### Flashing the Firmware (CLI)
You can flash the device directly from PlatformIO:
```powershell
pio run -e esp32c3 -t upload
```

Alternatively, you can flash using the **Flash** tab within the eenky IDE, which uses ESP Web Tools to push `firmware-factory.bin` over USB.

---

## 5. Releases & Versioning

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

## 6. Credits & Acknowledgments

eenk builds upon the work of open-source firmware projects, drivers, and runtime libraries. See [credits.md](credits.md) for the complete list of credits and software acknowledgments.
