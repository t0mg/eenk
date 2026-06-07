# EENK — Interactive Fiction Runtime for Xteink X4

Welcome to **EENK**, a custom firmware fork of the **Papyrix Reader** designed to run **InkCPP** interactive fiction stories on the Xteink X4 e-reader.

This project supports two primary build configurations managed via PlatformIO:
- **`native`**: A desktop simulation of the runtime, using an 800x480 SDL2 graphical window to simulate the e-ink display.
- **`esp32c3`**: The firmware build for the actual ESP32-C3 microcontroller hardware (simulated in Wokwi, outputting via VT100 Serial).

---

## 1. How to Compile Ink Stories

We provide a dedicated **EENK Story Compiler** desktop application to automatically convert your `.ink` source files into the optimized binary format required by InkCPP.

### Using the GUI
1. Navigate to `tools/eenk-compiler` and run `npm start` (or install the generated `.exe` from `tools/eenk-compiler/dist-electron`).
2. Drag and drop your `.ink` file into the compiler window.
3. The `.bin` file will be generated instantly in the same directory as your source file.

### Using the CLI
If you prefer a terminal workflow, you can run the compiler headless by passing the ink file path:
```powershell
cd tools/eenk-compiler
npm start ../../stories/hello_world.ink
# Or using the packaged executable:
.\dist-electron\win-unpacked\"EENK Story Compiler.exe" ../../stories/hello_world.ink
```

---

## 2. Desktop Simulation (`native` configuration)

The native configuration compiles a host executable that runs the compiled story interactively in an 800x480 SDL2 graphical window, rendering text with a bundled CP437 font to perfectly simulate the e-ink experience.

### Building the Native Target
Ensure that the MinGW compiler suite (`g++`) is in your PATH. If you use MSYS2, the compiler path is typically `C:\msys64\mingw64\bin`.

```powershell
# Add MinGW bin folder to PATH and run PlatformIO build
$env:PATH="C:\msys64\mingw64\bin;$env:PATH"
pio run -e native
```

### Running the Native Target
Once built, the executable is created at `.pio/build/native/program.exe`. The runtime standard libraries and SDL2 are statically linked, so you can execute the program directly with the compiled story file:

```powershell
# Run with the compiled story
.pio\build\native\program.exe stories/the_intercept.bin
```

Use `W/S` or the **Arrow Keys** to navigate choices, and **Enter** to confirm.


---

## 3. ESP32-C3 Target (`esp32c3` configuration)

The `esp32c3` environment builds the firmware partition for the actual physical device or Wokwi simulator. In this milestone, the display driver is stubbed out and replaced with a VT100 Serial renderer, allowing you to play the game inside the Wokwi Serial Monitor!

### Building the ESP32 Target
The firmware embeds the story binary directly into the flash `.rodata` section (configured via `board_build.embed_txtfiles = data/the_intercept.bin`). To compile the firmware, run:
```powershell
pio run -e esp32c3
```
This produces the firmware binaries under `.pio/build/esp32c3/firmware.bin`.

### Wokwi Simulation Behavior
Start the Wokwi simulation via the VS Code extension or web interface. The terminal output will boot the ESP32, mount the embedded data, print memory usage statistics, and drop you straight into the interactive story loop:

```text
=== EENK Interactive Fiction Runtime (ESP32-C3) ===
Free heap before init: 306388 bytes
[InkEngine] Story loaded — 150897 bytes
Free heap after load: 112008 bytes
...
```

Click on the Serial Terminal and use the keyboard to navigate through the story!
