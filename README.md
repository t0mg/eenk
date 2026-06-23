# EENK — Interactive Fiction Firmware for Xteink X4

Welcome to **EENK**, a custom firmware designed to run **Ink** interactive fiction stories on the Xteink X4 e-ink device, utilizing the [inkcpp](https://github.com/t0mg/inkcpp) C++ runtime.

This project supports two primary build configurations managed via PlatformIO:
- **`native`**: A desktop simulation of the runtime, using an 800x480 SDL2 graphical window to simulate the e-ink display for testing.
- **`esp32c3`**: The firmware build for the actual ESP32-C3 microcontroller hardware.

---

## 1. Project Architecture

The EENK ecosystem relies on two repositories working together through git submodules:

- **`eenk` (This repository)**: The core C++ firmware and SDL simulation runtime. It handles rendering to the e-ink display (or SDL window) and executing the compiled Ink binary using the `inkcpp` runtime.
- **`eenky` (IDE)**: The Electron-based editor and toolchain. It provides the UI to write Ink stories, compile them to JSON and then to `.bin`, run the SDL simulator from `eenk`, and finally flash the compiled firmware to the device via Web Serial.

> [!NOTE]
> `eenky` is included as a submodule in `tools/eenky`. Conversely, `eenky` includes `eenk` as a submodule to build the simulator backend. They operate in tandem.

---

## 2. Desktop Simulation (`native` configuration)

The native configuration compiles a host executable that runs the compiled story interactively in an 800x480 SDL2 graphical window, rendering text with a bundled CP437 font to perfectly simulate the e-ink experience. This executable is used as the simulation backend for the EENKY IDE.

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
- **`.pio/build/esp32c3/firmware-factory.bin`** (The merged bootloader + partition table + app, used by ESP Web Tools in the EENKY IDE)

### Flashing the Firmware (CLI)
You can flash the device directly from PlatformIO:
```powershell
pio run -e esp32c3 -t upload
```

Alternatively, you can flash using the **Flash** tab within the EENKY IDE, which uses ESP Web Tools to push `firmware-factory.bin` over USB.
