# FreeInk SDK Integration Analysis for eenk

## Background

eenk currently uses a **partial, hand-rolled HAL** over the Papyrix-era `EInkDisplay` / `GfxRenderer` / `InputManager` / `BatteryMonitor` libraries it inherited. FreeInk SDK is a clean re-architecture of those same Papyrix-lineage libraries — explicitly targeting the same hardware family and aiming to be drop-in compatible at the API level.

---

## What eenk currently owns vs. what FreeInk provides

| eenk layer | What it is | FreeInk equivalent |
|---|---|---|
| `lib/FreeInkDisplay` | A local copy of the FreeInkDisplay façade + SSD1677 + UC8179 drivers, with a hand-rolled `BoardConfig.h` shim | **Already the SDK** — just a vendored snapshot without the full device registry |
| `lib/BatteryMonitor` | ADC + BQ27220 + CW2017 fuel gauge driver | `libs/hardware/BatteryMonitor` — identical or very close |
| `lib/InputManager` | ADC ladder + digital button driver | `libs/hardware/InputManager` — drop-in compatible |
| `src/hal/esp32/x4/` | `EspEinkDisplay`, `EspAdcInput`, `EspSdStorage`, `HalInit`, `HalTypes` | Replaced by `BoardConfig::XTEINK_X4` profile + `FreeInkDisplay` + `InputManager` |
| `src/hal/esp32/x4pro/` | `EspEinkDisplay`, `EspDigitalInput`, `EspSdmmcStorage`, `HalInit`, `HalTypes` | Replaced by `BoardConfig::XTEINK_X4PRO` profile |
| `src/hal/sdl/` | `SDLDisplay`, `SDLInput`, `SDLStorage`, `mock/EInkDisplay.h` | **No FreeInk equivalent** — SDL is eenk-specific |
| `src/hal/IDisplay.h` / `IInput.h` / `IStorage.h` | eenk's own platform-agnostic interfaces (vtable) | FreeInk doesn't have a vtable HAL layer; it uses compile-time BoardConfig |
| `lib/GfxRenderer`, `lib/EpdFont`, etc. | Rendering stack | Not in FreeInk SDK; eenk-specific |

### Key observation: eenk's `lib/FreeInkDisplay` IS already FreeInk

The `lib/FreeInkDisplay` directory contains files (`FreeInkDisplay.h`, `FreeInkDisplay.cpp`, `EpdBus`, driver subdirs, `BoardConfig.h`) that match the SDK's `libs/display/FreeInkDisplay` **almost exactly**. This is already a vendored copy of the SDK's display layer — but frozen, without the full `BoardConfig` registry, and without the UC8253/X3 drivers.

The current `lib/FreeInkDisplay/include/BoardConfig.h` is a **custom shim** (151 lines) that hard-wires X4 Pro flags and omits the device registry entirely.

---

## Pros of adopting FreeInk SDK as a submodule

### 1. X3 support for free
This is the biggest win. FreeInk ships:
- `FREEINK_DEVICE_X3` board profile (792×528 / UC8253)
- `FREEINK_DRIVER_UC8279` for newer X3 units (UC8279d) 
- `XteinkDetect` library with canonical I²C fingerprinting (BQ27220 + DS3231 + QMI8658 on SDA=20/SCL=0) and display-bus probing
- BQ27220 I²C fuel gauge mode in `BatteryMonitor` (already in eenk's local copy, but SDK keeps it in sync)
- One ESP32-C3 binary with runtime device selection (`setDisplayX3()`)

eenk would get X3 support by:
1. Adding `-DFREEINK_DEVICE_X3=1` to `esp32c3` env
2. Calling `freeink::selectXteinkDevice()` at boot before display init
3. Propagating the 792×528 geometry to `GfxRenderer` / `IDisplay::getWidth/Height`

### 2. Eliminates the local copy maintenance burden
eenk currently maintains `lib/FreeInkDisplay` as a local snapshot, with a custom `BoardConfig.h` shim, and separate `BatteryMonitor` and `InputManager` copies. Any bug fix or driver improvement in the SDK (e.g. UC8179 waveform tuning, BQ27220 poll-rate fix) has to be manually applied. Using the submodule means `git submodule update --remote` picks up upstream improvements.

### 3. Per-batch controller detection (`applyXteinkDisplayController()`)
FreeInk's `XteinkDetect` handles the SSD1677 → UC8179 swap on X4/X4Pro and the UC8253 → UC8279d swap on X3 via NVS-first, then display-bus probe. eenk's current X4 Pro init does only a crude BUSY-pin idle-state heuristic (which the X4 Pro `EspEinkDisplay.cpp` uses). FreeInk's approach is more robust.

### 4. `PowerManager` — portable deep-sleep wakeup
eenk's `src/hal/esp32/x4/HalInit.cpp` uses `esp_deep_sleep_enable_gpio_wakeup` (C3-only), and the X4 Pro `HalInit.cpp` uses `esp_sleep_enable_ext0_wakeup` (S3 RTC). The SDK's `PowerManager` abstracts this with `SOC_PM_SUPPORT_EXT1_WAKEUP` guards, so both targets use the same call.

### 5. Cleaner `platformio.ini` — device × capability composition
FreeInk's build model (`-DFREEINK_DEVICE_X3=1 -DFREEINK_DEVICE_X4=1`) is more expressive than the current per-directory include path trick (`-Isrc/hal/esp32/x4`). Capabilities (touch, frontlight, SD variant) become explicit flags instead of per-directory file inclusions.

### 6. Upstream community alignment
CrossPoint uses FreeInk SDK. Being on the same SDK means shared waveform improvements, driver bug fixes, and new device ports (Sticky, de-link, Murphy) are available at submodule update time.

---

## Cons and risks

### 1. The SDL native build — the hardest part
FreeInk is **Arduino/ESP32-only**. `BoardConfig.h` includes `<Arduino.h>` and `<driver/gpio.h>` unconditionally. `FreeInkDisplay.h` includes `<Arduino.h>`, `<SPI.h>`, and `"../src/bus/EpdBus.h"`.

eenk's native build currently stubs these via `src/hal/sdl/mock/` (`Arduino.h`, `EInkDisplay.h`, `FS.h`). **The FreeInk SDK's `BoardConfig.h` would need an equivalent guard or separate stub** — the existing `#ifndef ARDUINO` guard in `FreeInkDisplay.cpp` is a start, but `BoardConfig.h` has no such guard.

Two workable approaches:
- **Keep the current SDLDisplay approach**: the native build continues to use `src/hal/sdl/mock/EInkDisplay.h` (the stub), and the FreeInk SDK is only compiled for ESP32 targets. The SDL build bypasses `lib_deps` that reference FreeInk via `lib_ignore` (already done: `lib_ignore = EInkDisplay`).
- **Extend the mock**: add `BoardConfig.h` mock stubs in `src/hal/sdl/mock/` to satisfy FreeInk includes — but this is fragile as the SDK evolves.

**Verdict**: Keep the SDL native build isolated from FreeInk exactly as today. The native build uses `src/hal/sdl/mock/EInkDisplay.h` and `SDLDisplay` wrapping it. FreeInk SDK is only in the ESP32 envs. This is clean and already works.

### 2. `GfxRenderer` is not part of FreeInk SDK
eenk's rendering stack (`GfxRenderer`, `EpdFont`, `ExternalFont`, text layout) is entirely eenk's own (inherited from Papyrix, but maintained independently). FreeInk SDK's `FreeInkUI` is a separate immediate-mode UI layer with its own bitmap font — **not a replacement for `GfxRenderer`**. Adopting FreeInk SDK does NOT mean replacing the rendering stack. `GfxRenderer` still takes `EInkDisplay&`, which is satisfied by `FreeInkDisplay` via the `EInkDisplay.h` compat shim.

### 3. The `BoardConfig` shim must go — but the transition is small
The current `lib/FreeInkDisplay/include/BoardConfig.h` defines `BoardConfig::ACTIVE` locally in each `EspEinkDisplay.cpp`. With the full SDK, `BoardConfig::ACTIVE` is initialized by the SDK's `BoardConfig.h` per-profile defaults, and `selectDevice()` swaps it. The `EspEinkDisplay.cpp` files currently do: `BoardConfig::ACTIVE = { "X4", ... }`. This would be replaced by `BoardConfig::selectDevice(Board::XteinkX4)` (or just using the SDK defaults).

### 4. `InputManager` API mismatch with eenk's `IInput`
eenk wraps `InputManager` behind `IInput` (a vtable interface with `pollInput()` → `ButtonEvent`). FreeInk's `InputManager` uses `update()` + `wasPressed()` / `wasReleased()` (edge-state model). The adapter layer in `EspAdcInput.cpp` translates between them. This adapter would remain — only the underlying `InputManager` version changes.

### 5. `esp32c3_updater` must stay minimal
The updater's `build_src_filter` only pulls in a subset of source files. Adding FreeInk SDK to the updater env could bloat it beyond the 1 MB limit. The updater should **not** use the FreeInk display driver — it uses `InputManager` only for the button check, and `SD.begin()` for firmware flash. The SDK's `InputManager` and `BoardConfig` could be used, but the display driver must be excluded. This requires care in the updater's `lib_deps` — either not including FreeInk display, or keeping the existing minimal approach.

### 6. Upstream maturity on the X3 UC8279 driver
The X3 UC8279d driver is explicitly marked **"Pending hardware validation"** in the SDK docs. This means X3 support via FreeInk may require contributor work to tune waveforms before it's production-ready. The UC8253 path (original X3) is more mature.

---

## The native SDL build — detailed assessment

The native build's position relative to FreeInk:

```
Native build today:
  src/hal/sdl/mock/EInkDisplay.h   ← stub (no SPI, no Arduino)
  lib/FreeInkDisplay                ← IGNORED (lib_ignore = EInkDisplay)
  SDLDisplay wraps _mockEink → GfxRenderer → SDL texture

After FreeInk SDK adoption:
  (Same)
  lib_ignore = EInkDisplay          ← still exclude the SDK display lib from native
  SDLDisplay continues to use src/hal/sdl/mock/EInkDisplay.h
  BoardConfig is never included in native (FreeInk's BoardConfig.h includes Arduino.h)
```

The native build is **unaffected** by the SDK adoption. The mock stays. The SDL simulator's geometry (800×480 today) would need to be made configurable if X3 (792×528) is ever simulated natively, but that's optional scope.

---

## Overall verdict: **Pros outweigh cons — adopt FreeInk SDK**

The key arguments:
1. eenk already **uses a copy of FreeInk SDK** (`lib/FreeInkDisplay`). The question is whether to maintain the copy or use the submodule.
2. X3 support is a strong concrete benefit: it adds an entire hardware SKU with minimal code change.
3. The native SDL build is fully insulated — no risk.
4. The updater is manageable with careful `lib_deps` scoping.
5. Keeping the custom shim `BoardConfig.h` means permanently diverging from upstream without benefit.

---

## Proposed refactoring — multi-step plan

The strategy is to replace eenk's local SDK copies with the FreeInk submodule **one library at a time**, verifying builds at each step, with no functional regression until the final X3 step.

### Step 0: Housekeeping (pre-work)
- Add `freeink-sdk` as a git submodule (e.g. `lib/freeink-sdk` → `t0mg/freeink-sdk@eenk-patches` branch, or `main` if no patches needed yet).
- Remove the `freeink-sdk-main/` temporary copy from the repo root.
- Add `freeink-sdk` to `.gitignore` exemption (submodule, not ignored).

### Step 1: Replace `lib/FreeInkDisplay` with the SDK's display library
- Point `EInkDisplay` lib_dep to `symlink://lib/freeink-sdk/libs/display/FreeInkDisplay` in ESP32 envs.
- Replace `lib/FreeInkDisplay/include/BoardConfig.h` shim with the SDK's full `BoardConfig.h` (via the submodule).
- Update `src/hal/esp32/x4/EspEinkDisplay.cpp` and `x4pro/EspEinkDisplay.cpp` to initialize `BoardConfig::ACTIVE` via `BoardConfig::selectDevice()` instead of the manual struct literal.
- Verify: `pio run -e esp32c3`, `pio run -e esp32s3`, `pio run -e native` all build.

### Step 2: Replace `lib/BatteryMonitor` with the SDK's BatteryMonitor
- Point `BatteryMonitor` lib_dep to the SDK.
- The API is already identical — no source changes expected.
- Verify builds.

### Step 3: Replace `lib/InputManager` with the SDK's InputManager
- Point `InputManager` lib_dep to the SDK.
- The `EspAdcInput`/`EspDigitalInput` adapters call `InputManager` via `wasPressed()` etc.; verify the API hasn't changed.
- The full `BoardConfig` registry now provides `POWER_BUTTON_PIN` correctly for both X4 and X4 Pro.
- Verify builds + golden screenshot tests pass.

### Step 4: Add `SDCardManager` from the SDK (optional improvement)
- eenk's `EspSdStorage` wraps `SD.begin()` + `SD.open()`. The SDK's `SDCardManager` provides the same but handles the SD rail release quirk (`releaseSdRail()`) correctly on boards where SD shares the display bus.
- Adopt SDK `SDCardManager` and remove `lib/FsAdapter` duplication.
- Verify SD card operations in simulator.

### Step 5: Simplify HAL init — use `PowerManager` for sleep/wake
- Replace `esp_deep_sleep_enable_gpio_wakeup` (C3) and `esp_sleep_enable_ext0_wakeup` (S3) in `HalInit.cpp` with `freeink::PowerManager::armPowerButtonWakeup()`.
- This is the MCU-portability improvement documented in `docs/consumer-mcu-portability.md`.
- Add `PowerManager=symlink://lib/freeink-sdk/libs/hardware/PowerManager` to ESP32 lib_deps.
- Verify sleep/wake on X4 and X4 Pro.

### Step 6: Add `XteinkDetect` + X3 device support
- Add `-DFREEINK_DEVICE_X3=1` to `esp32c3` env.
- Add `XteinkDetect=symlink://lib/freeink-sdk/libs/hardware/XteinkDetect` to `esp32c3` lib_deps.
- In `src/hal/esp32/x4/HalInit.cpp` (or early in `setup()`), call `freeink::selectXteinkDevice()` before display init. If it returns true (X3), call `display->setDisplayX3()`.
- Update `IDisplay::getWidth/Height` in the display wrapper to read from `BoardConfig::ACTIVE.displayWidth/Height` dynamically.
- Update `GfxRenderer` initialization to use the runtime-correct geometry.
- X3 also needs `BatteryMonitor::Bq27220Config` — the SDK `BatteryMonitor` already supports this; `HalInit::createBatteryMonitor()` selects mode based on whether X3 was detected.
- Test: build `esp32c3` env with both device flags. Golden screenshots remain at 800×480. X3 rendering at 792×528 can be a follow-up.

### Step 7 (optional): Add `XteinkDetect::applyXteinkDisplayController()`
- Replace the crude BUSY-pin heuristic in `x4pro/EspEinkDisplay.cpp` with `freeink::applyXteinkDisplayController()` for correct SSD1677 → UC8179 detection.
- The SDK's display-bus probe + NVS-first approach is more reliable than the idle-state heuristic.

### Step 8: Update AGENTS.md, README.md
- Update `lib/` table to reference FreeInk SDK submodule.
- Add X3 to supported hardware list.
- Note the `freeink-sdk` submodule in the repository architecture section.

---

## What does NOT change

- `lib/GfxRenderer`, `lib/EpdFont`, `lib/ExternalFont`, `lib/Hyphenation`, `lib/ArabicShaper`, `lib/ThaiShaper`, `lib/Utf8`, `lib/ScriptDetector`, `lib/Logging` — these are eenk's own rendering stack, not replaced by FreeInk.
- `src/hal/IDisplay.h`, `IInput.h`, `IStorage.h` — the vtable abstraction layer stays.
- `src/hal/sdl/` — the SDL native build is unchanged.
- `src/updater/` — minimal, explicit `build_src_filter` keeps it isolated.
- `lib/inkcpp` submodule — completely orthogonal.
- `lib/FreeInkUI` — NOT adopted (eenk uses GfxRenderer/EpdFont, not FreeInkUI).

---

## Open questions

1. **Branch policy**: Should eenk use `freeink-sdk@main` directly, or maintain a `t0mg/freeink-sdk` fork on an `eenk-patches` branch (mirroring the inkcpp pattern) in case local patches are needed?

2. **X3 runtime geometry**: When X3 is detected, `GfxRenderer` needs to be told the display is 792×528 not 800×480. `GfxRenderer::setOrientation()` currently sets layout implicitly. Is a `setDisplaySize()` call needed, or does it derive from `EInkDisplay::getDisplayWidth()`?

3. **Updater scope**: The `esp32c3_updater` env currently uses `InputManager` from `lib/InputManager`. Should it stay on the local copy (simpler, known size) or move to the SDK? The updater must fit in 1 MB.

4. **X3 UC8279d waveforms**: The UC8279d driver is explicitly "Pending hardware validation." Should X3 support initially target UC8253-only units (safer), with UC8279d as a follow-up once the FreeInk SDK has validated it?
