# AGENTS.md — eenk Project Guide

This file provides context and instructions for AI coding agents working on the eenk ecosystem.

---

## Ecosystem Overview

The eenk project is an interactive fiction runtime for the **Xteink X4** e-ink device. It consists of three repositories:

| Repository | Description |
|-----------|-------------|
| **[eenk](https://github.com/t0mg/eenk)** | Core C++ firmware for the ESP32-C3 hardware and SDL desktop simulator. This is the **primary** repository. |
| **[eenky](https://github.com/t0mg/eenky)** | Electron + Vue 3 IDE for authoring Ink stories, running the SDL simulator, and flashing firmware to the device via USB. Fork of inkle's Inky. |
| **[inkcpp](https://github.com/t0mg/inkcpp)** | Fork of the inkcpp C++ Ink runtime with eenk-specific patches (branch: `eenk-patches`). Used as a submodule by both eenk and eenky. |

---

## Repository Architecture & Submodule Rules

The repositories are cross-linked via git submodules:

```
eenk (primary repo)
├── lib/freeink-sdk  → Free-Ink/freeink-sdk (main branch)
└── lib/inkcpp       → t0mg/inkcpp (eenk-patches branch)
```

eenky is a **separate repository** (not a submodule of eenk). It has its own submodule structure:

```
eenky
├── eenk/    → t0mg/eenk (submodule pointing at the primary repo)
└── inkcpp/  → t0mg/inkcpp (eenk-patches branch)
```

> [!IMPORTANT]
> After making changes in eenk that affect the simulator binary, update the eenky submodule pointer and rebuild:
> ```sh
> cd ../eenky     # in the eenky repo
> git submodule update --remote eenk
> npm run setup
> ```

---

## eenk Repository (Firmware & Simulator)

### Build System

eenk uses **PlatformIO** with four build environments defined in `platformio.ini`:

| Environment | Target | Purpose |
|------------|--------|---------|
| `native` | Host (MinGW/SDL2) | Desktop simulator, renders to SDL2 window |
| `esp32c3` | ESP32-C3 | Main firmware for Xteink X3 and X4 hardware (app0) |
| `esp32c3_updater` | ESP32-C3 | Minimal OTA updater firmware for X4 (app1) |
| `esp32s3` | ESP32-S3 | Main firmware for Xteink X4 Pro hardware (app0) |

**Build commands:**
```sh
pio run -e native          # Desktop simulator
pio run -e esp32c3         # Main firmware (X4)
pio run -e esp32s3         # Main firmware (X4 Pro)
pio run -e esp32c3_updater # OTA updater (X4)
pio test -e native         # Unit tests (native only)
```

> [!NOTE]
> The X4 Pro has **no separate updater environment** — it uses symmetric A/B OTA partitions (both `app0` and `app1` are full ~7.9 MB firmware slots). OTA on the X4 Pro is handled by swapping the active slot via `esp_ota_set_boot_partition` directly from the main firmware.

> [!IMPORTANT]
> **After making changes, always build `native`, `esp32c3`, and `esp32s3` targets** to verify cross-platform compatibility. The native build uses host g++ with SDL2; the ESP32 builds use the ESP-IDF Arduino framework.

### Partition Layout

The ESP32-C3 (X4) and ESP32-S3 (X4 Pro) both use 16 MB flash chips, but their partition schemes differ.

**X4 (16 MB) Layout (`partitions.csv`):**
| Partition | Type | Offset | Size | Purpose |
|-----------|------|--------|------|---------|
| `nvs` | data | 0x9000 | 20 KB | Non-volatile storage (settings, boot state) |
| `otadata` | data | 0xE000 | 8 KB | OTA boot selection metadata |
| **`app0`** | app (ota_0) | 0x10000 | **7 MB** | Main firmware — full interactive fiction runtime |
| **`app1`** | app (ota_1) | 0x710000 | **1 MB** | OTA Updater — minimal recovery/update firmware |
| `ink_cache` | data (FAT) | 0x810000 | ~7.9 MB | Story file cache (FAT filesystem) |
| `coredump` | data | 0xFF0000 | 64 KB | Crash dump storage |

**X4 Pro (16 MB) Layout (`partitions_x4pro.csv`):**
| Partition | Type | Offset | Size | Purpose |
|-----------|------|--------|------|---------|
| `nvs` | data | 0x9000 | 20 KB | Non-volatile storage (settings, boot state) |
| `otadata` | data | 0xE000 | 8 KB | OTA boot selection metadata |
| **`app0`** | app (ota_0) | 0x10000 | **~7.9 MB** | Main firmware — full interactive fiction runtime |
| **`app1`** | app (ota_1) | 0x800000 | **~7.9 MB** | Alternate OTA partition — full runtime (A/B updates) |
| `coredump` | data | 0xFF0000 | 64 KB | Crash dump storage |

**Key constraints:**
- `app0` holds the full runtime with the Ink engine, UI, fonts, and rendering. Most firmware development happens here.
- On the X4 Pro, `app0` and `app1` are identical in size (~7.9 MB) for full A/B OTA updates. There is no `ink_cache` partition because stories are loaded directly into PSRAM.
- On the X4 (no PSRAM), `app1` (1 MB) is the **updater partition** — an intentionally minimal firmware that handles OTA updates and SD card firmware flashing.

> [!CAUTION]
> **The updater partition (`app1`, `src/updater/`) must remain extremely lean and robust.** It is the device's recovery mechanism. Before making any change to the updater code, always ask:
> 1. Is this change absolutely necessary?
> 2. Could a bug here brick the device?
> 3. Will the resulting binary still fit in 1 MB?
>
> The updater only links a small subset of source files — see the `build_src_filter` in the `[env:esp32c3_updater]` section of `platformio.ini`.

### Source Code Layout

```
src/
├── main.cpp                  # Application entry point, main loop
├── engine/
│   ├── InkEngine.cpp/.h      # Core Ink story execution engine
├── hal/
│   ├── IDisplay.h            # Display hardware abstraction interface
│   ├── IInput.h              # Input hardware abstraction interface
│   ├── IStorage.h            # Storage hardware abstraction interface
│   ├── esp32/                # ESP32-C3 hardware implementations
│   └── sdl/                  # SDL2 desktop simulation implementations
│       └── mock/             # Mock Arduino/ESP headers for native build
├── os/
│   ├── AppSettings.cpp/.h    # User preferences (fonts, margins, sleep)
│   ├── BootManager.cpp/.h    # Boot mode management (NVS-based)
│   └── SdFontCatalogue.cpp/.h # SD card font discovery
├── serial/
│   └── SerialFileServer.cpp/.h # USB serial file transfer protocol
├── ui/
│   ├── SystemUI.cpp/.h       # Top-level UI coordinator
│   ├── Library.cpp/.h        # Story library browser
│   ├── SettingsView.cpp/.h   # Settings menu
│   ├── BatteryWidget.cpp/.h  # Battery indicator
│   ├── HeaderWidget.cpp/.h   # Header bar
│   ├── FooterWidget.cpp/.h   # Footer bar
│   └── StoryMetadata.cpp/.h  # Story metadata parsing
└── updater/
    ├── UpdaterMain.cpp       # OTA updater entry point (app1 only)
    └── WifiProvisioner.cpp/.h # WiFi credential reader
```

> [!NOTE]
> **Keep this layout up to date.** When adding new source files or directories, update the tree above so agents can orient themselves.

### Libraries (`lib/`)

| Library | Purpose |
|---------|---------|
| `ArabicShaper` | Arabic text shaping for RTL rendering |
| `EpdFont` | Bitmap font format and rendering |
| `ExternalFont` | SD card font loading |
| `FsAdapter` | Filesystem abstraction (SD/SPIFFS/Stdio) |
| `GfxRenderer` | High-level graphics renderer (text layout, drawing primitives) |
| `Hyphenation` | Text hyphenation for justified text |
| `Logging` | Debug logging utilities |
| `ScriptDetector` | Script/language detection (Latin, Arabic, Thai, etc.) |
| `ThaiShaper` | Thai text shaping |
| `Utf8` | UTF-8 encoding/decoding utilities |
| `freeink-sdk` | **Submodule** — Hardware abstraction SDK (BoardConfig, EInkDisplay, InputManager, BatteryMonitor, PowerManager, XteinkDetect) |
| `inkcpp` | **Submodule** — Custom inkcpp Ink runtime |

> [!NOTE]
> **Keep this table up to date.** When adding new libraries under `lib/`, add a row here.

### Golden Screenshot Testing

When making **display-related changes**, always add or update golden screenshot tests in `test/test_main.cpp`:

1. The tests use the native simulator to render UI components.
2. Output screenshots are saved as `.bmp` files to `test/golden/`.
3. Golden reference files are tracked in git (e.g., `test_battery_widget.bmp`, `test_library.bmp`).
4. There is **no automated pixel comparison** — after running the tests, use `git diff` (or your git client) to check whether any golden files have changed.

**Workflow:**
```sh
pio test -e native            # Run all tests — regenerates golden screenshots
git diff test/golden/          # Check for visual changes
```
If diffs are intentional (the UI changed), commit the updated golden files. If not, fix the regression.

Existing golden tests cover: battery widget, external fonts, built-in fonts, game library, modal dialogs, settings view, story player, and system UI.

### Papyrix Heritage

eenk uses a lot of code originally from the [papyrix-reader](https://github.com/bigbag/papyrix-reader) firmware (though it is NOT a git fork). When implementing new features, always consider:
- Does papyrix already implement this feature? If so, study its approach.
- Are there relevant utility classes or hardware drivers in papyrix that could be adapted?
- The BootManager's NVS strategy and display driver architecture were influenced by papyrix patterns.

### Hardware Notes

- **Supported Devices**: Xteink X3 (792×528, UC8253/UC8279), Xteink X4 (800×480, SSD1677), Xteink X4 Pro (800×480, SSD1677/UC8179)
- **Display**: Monochrome e-ink display driven via FreeInk SDK facade
- **MCU**: ESP32-C3 (X3/X4) or ESP32-S3 (X4 Pro)
- **Storage**: SD card (SPI/SDMMC) + 16 MB flash
- **Power**: Latching power circuit — deep sleep physically cuts power, destroying RTC memory. Always test sleep/wake on battery, NOT USB (USB keeps the MCU powered, giving false results).

### Story Format & Sidecar Files

Stories go through a two-stage compilation pipeline in eenky:

1. **inklecate** compiles `.ink` source to `.json` (standard Ink JSON).
2. **inkcpp_cl** compiles `.json` to `.bin` (compact binary for the inkcpp runtime).
3. eenky then **prepends a 128-byte metadata header** (`StoryMetadata` in `src/ui/StoryMetadata.h`) containing title, author, compile timestamp, flags, and an optional font hint. The device reads this header to populate the game library without loading the full story.

**Sidecar files** are placed alongside the `.bin` on the SD card:

- **Fonts**: `.epdfont` files converted from `.ttf` by eenky at compile time. Font resolution is multi-layered:
  1. Check built-in font tokens (e.g. `sans`, `serif`).
  2. Search the story's own folder on SD (e.g. `/eenk/mystory/myfont.epdfont`).
  3. Search the global `/fonts/` folder on SD.
  4. Fall back to the user's device font setting.
  
  Variants (`-bold`, `-italic`, `-bolditalic`) follow the same resolution chain; missing variants trigger synthetic fallbacks.

See `WritingForEenk.md` for the full authoring-facing documentation of metadata tags, fonts, and SD card layout.

---

## eenky Repository (IDE)

### Technology Stack

| Layer | Technology |
|-------|-----------|
| Desktop framework | Electron |
| Frontend | Vue 3 + Vite |
| Code editor | CodeMirror 6 with Ink language support |
| State management | Pinia |
| Backend tests | Mocha + Chai |
| Frontend tests | Vitest + Vue Test Utils |
| Ink compilation | inklecate (`.ink` → `.json`) + inkcpp_cl (`.json` → `.bin`) |

### Key eenky Commands

```sh
# From app/ directory:
npm run setup   # Build/copy simulator and compiler binaries into place
npm start       # Launch dev mode (Vite + Electron concurrently)
npm run test    # Run Mocha unit tests

# From app/renderer/ directory:
npm run test    # Run Vitest frontend tests
npm run dev     # Vite dev server only
npm run build   # Production build of the Vue renderer
```

> [!IMPORTANT]
> **Run `npm run setup`** (from the `app/` directory) whenever the parent `eenk` repo has changes that affect the native simulator binary. This rebuilds and copies `eenk-sim.exe` and `inkcpp_cl.exe` into the expected locations.

> [!NOTE]
> If you have **uncommitted changes** in the primary `eenk` repository that you want to test in the simulator, `npm run setup` will not pick them up (it pulls and builds the submodule from HEAD). Instead, build the native target manually in the primary `eenk` repo and copy the binary directly:
> ```powershell
> # From eenk/ directory
> pio run -e native
> Copy-Item -Path ".pio\build\native\program.exe" -Destination "tools\eenky\app\main-process\ink\win\eenk-sim.exe" -Force
> ```

### IDE Architecture

```
eenky/
├── app/
│   ├── main-process/
│   │   ├── main.js            # Electron main process entry
│   │   ├── appmenus.js        # Application menus & keyboard shortcuts
│   │   ├── projectWindow.js   # Project/editor window management
│   │   ├── eenkCompiler.js    # ink → json → bin compilation pipeline
│   │   ├── simulator.js       # SDL simulator process management
│   │   ├── inklecate.js       # inklecate process wrapper
│   │   ├── inkSnippets.js     # Code snippet templates
│   │   ├── preload.js         # Electron preload (IPC bridge)
│   │   ├── deviceWindow.js    # USB device management window
│   │   ├── aboutWindow.js     # About dialog
│   │   └── i18n/              # Internationalization strings
│   ├── renderer/              # Vue 3 + Vite frontend
│   │   ├── src/               # Vue components and stores
│   │   ├── public/            # Static assets
│   │   └── vitest.config.js   # Frontend test configuration
│   ├── test/                  # Mocha backend tests
│   └── resources/             # Documentation, icons, assets
├── scripts/
│   ├── setup.js               # Builds and copies backend binaries
│   └── copy-sim.js            # Copies simulator binary
├── bin/                       # Platform-specific compiled binaries
├── eenk/                      # Submodule → t0mg/eenk
└── inkcpp/                    # Submodule → t0mg/inkcpp
```

### Binary Resolution

The IDE expects these backend binaries in `app/main-process/ink/<platform>/`:

| Binary | Source | Purpose |
|--------|--------|---------|
| `inklecate_*` | Bundled from Inky fork | Converts `.ink` → `.json` |
| `inkcpp_cl` | Built from `inkcpp/` submodule | Converts `.json` → `.bin` for hardware |
| `eenk-sim` | Built from `eenk/` submodule via PlatformIO | Pixel-accurate e-ink simulator |

### Testing Guidelines

- Write unit tests for every new feature.
- Increase test coverage where it's lacking — especially around the compilation pipeline and serial communication.
- Backend tests (Mocha) go in `app/test/`.
- Frontend tests (Vitest) go alongside Vue components in `app/renderer/src/`.

### Keyboard Shortcuts

Shortcuts are defined in `app/main-process/appmenus.js`. Key shortcuts include `Ctrl+P`/`Cmd+P` for Goto Anything.

> [!IMPORTANT]
> When adding or modifying keyboard shortcuts, always:
> 1. Update the shortcuts definition in `appmenus.js`.
> 2. Verify the Shortcuts help window (`app/renderer/src/components/Modals.vue`) is properly updated to reflect the change.
> 3. Check for conflicts with existing Electron or CodeMirror shortcuts.
> 4. Update the shortuts table in `eenky/docs/pages/eenky.md` in the root repository.

---

## General Conventions

### Documentation

> [!IMPORTANT]
> Always check whether `README.md` files need updating after making feature changes — both in eenk and eenky. Also consider updating `WritingForEenk.md` if authoring-facing features change (tags, fonts, Ink extensions).

### Build Verification Checklist

After making firmware changes in eenk:
- [ ] `pio run -e native; Copy-Item -Path ".pio\build\native\program.exe" -Destination "tools\eenky\app\main-process\ink\win\eenk-sim.exe" -Force` builds successfully
- [ ] `pio run -e esp32c3` builds successfully
- [ ] `pio run -e esp32s3` builds successfully
- [ ] `pio test -e native` passes (golden screenshots match or are intentionally updated)
- [ ] If updater was touched: `pio run -e esp32c3_updater` builds and binary fits in 1 MB

### Commit Workflow

1. Make firmware changes in the **primary eenk repo** only.
2. If changes affect the simulator, update the eenky submodule pointer and run `npm run setup` in eenky.
3. If inkcpp was modified, update the submodule pointer in both eenk and eenky.
4. Test native, esp32c3, and esp32s3 targets before committing.

### Platform Macros

| Macro | When Defined | Use For |
|-------|-------------|---------|
| `PLATFORM_ESP32` | ESP32-C3 & ESP32-S3 builds | Hardware-specific code (GPIO, SPI, NVS, I2C) |
| `PLATFORM_NATIVE` | Native/SDL builds | Desktop simulation stubs and SDL rendering |
| `HAS_SD_CARD` | ESP32-C3 builds | SD card filesystem access |

### Code Style

- C++17 (`-std=gnu++17`) across all targets.
- Use the HAL interfaces (`IDisplay`, `IInput`, `IStorage`) to keep hardware-specific code isolated in `hal/esp32/` and `hal/sdl/`.
- Keep the inkcpp runtime compiled with `-DINKCPP_NO_STD -DINKCPP_NO_RTTI -DINKCPP_NO_EXCEPTIONS` on ESP32 for size.
- Use `#ifdef PLATFORM_ESP32` / `#ifdef PLATFORM_NATIVE` guards for platform-specific code in shared files.
