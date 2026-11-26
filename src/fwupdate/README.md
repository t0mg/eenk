# eenk SD firmware updater (locked X4 only)

Self-update path for USB-locked Xteink X4 units (`[env:esp32c3_locked]`), where
the factory eFuse disables ROM download mode and `esp_ota_set_boot_partition()`
fails its image verify. Same safety contract as CrossPoint: validate the image
end-to-end, stream it into the **inactive** OTA app slot, flip otadata raw.
Bootloader, partition table and the running slot are never touched. NVS is
shared with CrossPoint (its `devid` secret plus eenk's
`boot`/`settings`/`ink_cache` namespaces, no collisions) and is never
erased in locked builds.

## File provenance & licensing

Synced from [`crosspoint-reader @ 209cb53a`](https://github.com/crosspoint-reader/crosspoint-reader)
(`fix: don't count blocked activity time as idle (#4538)`), Copyright (c) 2025
CrossPoint Reader organization, licensed under the MIT License (see `LICENSE`
in this directory).

| File | Origin | Rule |
|---|---|---|
| `FirmwareFlasher.h/.cpp` | `src/network/` verbatim | byte-identical, never edit |
| `OtaBootSwitch.h/.cpp` | `src/network/` verbatim | byte-identical, never edit |
| `FirmwareBoardTag.h/.cpp` | `src/network/` verbatim | byte-identical, never edit |
| `skip_efuse_blk_check.c` | `src/platform/` verbatim | byte-identical, never edit |
| `compat/HalStorage.h` | eenk-only shim | extend here instead of forking the flasher |
| `SdFirmwareUpdate.h/.cpp` | eenk-only UI flow | mirrors `SdFirmwareUpdateActivity` states |
| `README.md` (this file) | eenk-only | — |

Re-sync procedure: re-copy the 7 verbatim files, `cmp` each against upstream,
then confirm the assumptions below still hold (board tag, partition layout,
`LOG_*` arity, `HalFile` API surface).

## Why the verbatim copies compile unmodified

- `<Logging.h>` resolves to eenk's own `lib/Logging` (same `LOG_*(origin,
  format, ...)` API family; nothing else in eenk uses it). The locked env sets
  `-DENABLE_SERIAL_LOG -DLOG_LEVEL=2` so updater diagnostics reach serial.
- `<HalStorage.h>` resolves to `compat/` via the locked env's
  `-Isrc/fwupdate/compat`. Minimal `HalFile` + `Storage` over the already
  mounted Arduino `SD` driver — only what the flasher calls.
- `<BoardConfig.h>` is the shared freeink-sdk header; the tag derives from
  `-DFREEINK_DEVICE_X4=1 -DFREEINK_DEVICE_X3=1`, which both firmwares set on
  their C3 builds, so both embed **`CROSSPOINT-BOARD-V1:x4;`** and accept each
  other's images. Untagged images pass either way (guard rejects only
  present-and-mismatched tags).
- `-Wl,--wrap=bootloader_common_check_efuse_blk_validity` (locked env only)
  activates `skip_efuse_blk_check.c`, same as CrossPoint's `platformio.ini`.
- `src/fwupdate/` is compiled **only** in `[env:esp32c3_locked]` (every other
  env excludes it via `build_src_filter`), so stock/native builds are
  unaffected and the tag stays out of stock eenk images.

## Deliberate deviations from CrossPoint's UI

CrossPoint's `Activity` framework (async sub-activities, render task) doesn't
exist in eenk, whose UI is blocking/modal (`Library::run()`,
`SettingsView::run()`, `SystemUI::*`). The flow keeps the same states
(PICK → VALIDATE → CONFIRM → UPDATE → SUCCESS/FAILED, recovery never exits),
with these adaptations:

1. **Flat recursive picker** instead of a browsing file browser: all `*.bin`
   (case-insensitive, hidden files + `System Volume Information` skipped) from
   `/` down to 3 levels, max 64 entries, sorted, shown in a `showMenuModal`
   list as `name.bin (3.3 MB)`. eenk's `Library` only scans `/stories` +
   `/books` for story content, so a dedicated scan was required. Depth 3 keeps
   open dir handles under the SD driver's 8-file limit.
2. **Progress every 5%** instead of every 1%: each step is an e-ink full
   refresh (~1s); per-percent would add minutes to a ~3.4MB flash.
3. **"Do not power off"** lives in the confirmation dialog text: eenk's
   `LoadingWidget` progress screen has no sub-line slot.
4. **Recovery entry**: `Up` sampled in `HalInit::earlyBootCheck()` on cold boots
   only (`POWERON`/`DEEPSLEEP`-wake/`EXT`; a warm `ESP.restart()` boots
   normally even with `Up` held, mirroring CrossPoint's power-button-wake
   gate) sets a flag; `main.cpp` runs the updater after display/input/SD init,
   before boot-mode dispatch (CrossPoint samples then routes after init the
   same way). Button only — no `/firmware.bin` auto-trigger. Cancel/failure
   loop back to the picker; with no `.bin` present it shows the message and
   retries (re-mounting first, so an SD card inserted after boot is picked up).
   A power-button long-press deep-sleeps from anywhere (the e-ink modals ignore
   `SLEEP`, so input is routed through a decorator); the next power-on without
   `Up` boots normally, so recovery can never trap the user.
5. **Settings entry** replaces the `REBOOT_UPDATER` row in the same slot
   (`SD_FIRMWARE_UPDATE`, locked builds only); stock builds keep `bootUpdater()`.
