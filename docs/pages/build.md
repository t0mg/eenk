<div class="page-content">

# eenk development

Everything you need to build the eenk firmware from source and set up the eenky IDE for development.

> Note: **Just want to flash a device?** Skip the build environment entirely and use the [Web Flasher](../flasher/) to install pre-built firmware in minutes.

## Prerequisites

Before you begin, ensure the following are installed on your system:

| Tool | Required For | Notes |
|------|-------------|-------|
| [Git](https://git-scm.com/) | All | With submodule support |
| [Python 3.8+](https://www.python.org/) | PlatformIO + scripts | `python` must be in PATH |
| [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/) | Firmware builds | `pip install platformio` |
| [MinGW-w64 / g++](https://www.msys2.org/) | Native simulator (Windows) | MSYS2 recommended |
| [SDL2](https://www.libsdl.org/) | Native simulator | Via MSYS2 on Windows |
| [Node.js 18+](https://nodejs.org/) | eenky IDE | LTS recommended |

---

## Cloning the repository

eenk uses git submodules for its dependencies. Always clone recursively:

```powershell
git clone --recurse-submodules https://github.com/t0mg/eenk.git
cd eenk
```

If you already cloned without `--recurse-submodules`, initialize them manually:

```powershell
git submodule update --init --recursive
```

---

## Building the desktop simulator (`native`)

> Note: currently only ever tested on Windows (MSYS2).

The native build compiles an SDL2 desktop application that simulates the e-ink display at 800×480px. This is the fastest way to test firmware changes without hardware.

**Windows (MSYS2):**
```powershell
# Add MinGW to PATH, then build
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
pio run -e native
```

**Linux / macOS:**
```bash
# Install SDL2 via package manager first:
# sudo apt install libsdl2-dev   (Debian/Ubuntu)
# brew install sdl2               (macOS)
pio run -e native
```

The output binary is at `.pio/build/native/program.exe` (Windows) or `.pio/build/native/program` (Linux/macOS).

Run it with a compiled story:
```powershell
.pio\build\native\program.exe path\to\story.bin
```
Use `W`/`S` or arrow keys to navigate, `Enter` to confirm.

---

## Building the firmware

### Xteink X3 / X4 (`esp32c3`)

```powershell
pio run -e esp32c3
```

Outputs:
- `.pio/build/esp32c3/firmware.bin` — app partition only
- `.pio/build/esp32c3/firmware-factory.bin` — merged (bootloader + partitions + app), ready for Web Flasher

### Xteink X3 / X4 OTA Updater (`esp32c3_updater`)

The partition table contains a separate 1 MB updater partition (`app1`) to fit a minimal recovery firmware that can flash an OTA update from SD. Build the updater *before* the main firmware if you want it included in the factory image:

```powershell
pio run -e esp32c3_updater
pio run -e esp32c3
```

### Xteink X4 Pro (`esp32s3`)

```powershell
pio run -e esp32s3
```

Outputs:
- `.pio/build/esp32s3/firmware-factory.bin` — merged factory image for X4 Pro

---

## Runing tests

Unit tests with unsufficient coverage run against the native simulator and the simulator is used to generate "golden" screenshots testing various UI elements, which are saved into `test/golden/`:

```powershell
pio test -e native
```

Check for unintended visual regressions after display-related changes:

```powershell
git diff test/golden/
```

---

## Flashing to Hardware

Connect your device via USB and run:

```powershell
pio run -e esp32c3 -t upload
```

or for the X4Pro:

```powershell
pio run -e esp32s3 -t upload
```

# eenky IDE development

eenky is the companion Electron-based IDE for authoring Ink stories, running the simulator, and managing the hardware device.

## Cloning the repository

The eenky repository has eenk as a git submodule in order to build the simulator, and import shared css styles and documentation files.

```powershell
git clone --recurse-submodules https://github.com/t0mg/eenky.git
cd eenky/app
npm install
npm run setup   # Builds and copies the simulator binary into place
npm start       # Launch the IDE in development mode
```

> Note: If you make changes in the eenk submodule that affect the `native` environment target and want to test them in eenky, you can manually rebuild and copy and rename the binary `program.exe` output binary into `app/main-process/ink/win/eenk-sim.exe` (Windows example).

## Packaging for production

eenky uses electron-builder to create distributable installers and binaries. To build a production package:

```powershell
# 1. Build the Vue renderer
cd app/renderer
npm run build
cd ..

# 2. Package the app
#    This creates the installer for your current operating system
npm run dist
```

</div>
