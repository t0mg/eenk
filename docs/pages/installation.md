<div class="page-content">

# Installation

Everything you need to build eenk firmware from source and set up the eenky IDE.

<div class="info-box">
  <strong>Just want to flash a device?</strong> Skip the build environment entirely and use the <a href="../flasher/index.html">Web Flasher</a> to install pre-built firmware in minutes.
</div>

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

## 1. Clone the Repository

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

## 2. Build the Desktop Simulator (`native`)

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

## 3. Build Firmware for Hardware

### Xteink X3 / X4 (`esp32c3`)

```powershell
pio run -e esp32c3
```

Outputs:
- `.pio/build/esp32c3/firmware.bin` — app partition only
- `.pio/build/esp32c3/firmware-factory.bin` — merged (bootloader + partitions + app), ready for Web Flasher

### Xteink X4 OTA Updater (`esp32c3_updater`)

The X4 has a separate 1 MB updater partition (`app1`). Build the updater *before* the main firmware if you want it included in the factory image:

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

## 4. Run Unit Tests

Unit tests run against the native simulator and regenerate golden screenshots in `test/golden/`:

```powershell
pio test -e native
```

Check for unintended visual regressions after display-related changes:

```powershell
git diff test/golden/
```

---

## 5. Install the eenky IDE

eenky is the companion Electron-based IDE for authoring Ink stories, running the simulator, and managing your device.

```powershell
cd tools/eenky/app
npm install
npm run setup   # Builds and copies the simulator binary into place
npm start       # Launch the IDE in development mode
```

<div class="info-box">
  <strong>After firmware changes:</strong> Run <code>npm run setup</code> again (or manually copy the simulator binary) to update eenky with your latest native build.
</div>

```powershell
# Quick copy after a native build
pio run -e native
Copy-Item ".pio\build\native\program.exe" "tools\eenky\app\main-process\ink\win\eenk-sim.exe" -Force
```

---

## 6. Flash to Hardware

### Via Web Flasher (recommended)

Use the [Web Flasher](../flasher/index.html) tab — no drivers or CLI tools needed.

### Via PlatformIO

Connect your device via USB and run:

```powershell
pio run -e esp32c3 -t upload
```

### Via eenky IDE

The eenky Device Manager includes a **Flash Firmware** tab with the same wizard-style flasher embedded directly in the IDE.

</div>
