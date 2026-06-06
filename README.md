# EENK — Interactive Fiction Runtime for Xteink X4

Welcome to **EENK**, a custom firmware fork of the **Papyrix Reader** designed to run **InkCPP** interactive fiction stories on the Xteink X4 e-reader.

This project supports two primary build configurations managed via PlatformIO:
- **`native`**: A desktop simulation of the runtime, using an 800x480 SDL2 graphical window to simulate the e-ink display.
- **`esp32c3`**: The firmware build for the actual ESP32-C3 microcontroller hardware (simulated in Wokwi, outputting via VT100 Serial).

---

## 1. How to Compile Ink Stories

Before running either configuration, you must compile your `.ink` source files into the optimized binary format required by InkCPP.

The compilation is a two-step process:
1. **Compile `.ink` to `.json`** using `inklecate` (located in `tools/inklecate`).
2. **Compile `.json` to `.bin`** using the custom compiler tool `inkcpp_cl` (built in `lib/inkcpp/build/inkcpp_cl`).

### Step 1: `.ink` ➡️ `.json`
Run the following command from the project root:
```powershell
.\tools\inklecate\inklecate.exe -j -o stories/hello_world.ink.json stories/hello_world.ink
```

### Step 2: `.json` ➡️ `.bin`
Run the `inkcpp_cl` tool. *Note: As this tool is built using MinGW-w64, make sure the MinGW binary folder is in your PATH.*
```powershell
# In PowerShell:
$env:PATH="C:\msys64\mingw64\bin;$env:PATH"
.\lib\inkcpp\build\inkcpp_cl\inkcpp_cl.exe -o stories/hello_world.bin stories/hello_world.ink.json
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
