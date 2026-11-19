#pragma once
// eenk — SdFirmwareUpdate
// SD-card firmware updater for USB-locked X4 units (EENK_LOCKED_BUILD only).
//
// eenk-native, blocking reimplementation of crosspoint-reader's
// SdFirmwareUpdateActivity flow (PICK -> VALIDATE -> CONFIRM -> UPDATE ->
// SUCCESS/FAILED), using eenk widgets (SystemUI showMenuModal /
// showConfirmDialog / showLoading / showMessage). The actual flashing reuses
// the verbatim-copied firmware_flash::flashFromSdPath, so the on-flash safety
// contract (validate -> raw OTA write -> raw otadata flip) is identical.
//
// Recovery semantics mirror CrossPoint: with recoveryMode=true the flow never
// returns to the caller — cancel/failure loop back to the picker — and works
// with zero .bin files present (shows "No .bin files found" + retry).
// A power-button long-press deep-sleeps from anywhere (unlike CrossPoint,
// whose Activity framework keeps SLEEP working globally); the next power-on
// without UP boots normally, so recovery can never trap the user.

#include <string>
#include <vector>

#include "hal/IDisplay.h"
#include "hal/IInput.h"

class SdFirmwareUpdate {
 public:
  static constexpr int kMaxScanDepth = 3;  // root + 3 levels (SD max-open-files safe)
  static constexpr size_t kMaxEntries = 64;

  // Blocking. recoveryMode=false returns on cancel/failure (caller repaints);
  // recoveryMode=true only leaves via ESP.restart() or power-button sleep.
  static void run(IDisplay& display, IInput& input, bool recoveryMode);

 private:
  // Recursive *.bin scan from SD root (depth <= kMaxScanDepth, capped at
  // kMaxEntries, sorted). Mirrors the PickFirmware filter: extension .bin
  // (case-insensitive), hidden files and "System Volume Information" skipped.
  static void scanForFirmware(std::vector<std::string>& outPaths);

  // Pre-confirmation integrity check. Mirrors
  // SdFirmwareUpdateActivity::validateFirmware: size vs next-OTA-partition +
  // firmware_flash::validateImageFile, with CrossPoint's English error strings.
  static bool validateFirmware(const std::string& path, std::string& outError);

  // Block until the user presses CONFIRM/BACK/QUIT (fresh press, not the one
  // that led here). A power long-press (SLEEP) deep-sleeps via sleepNow().
  static void waitForDismiss(IInput& input, IDisplay& display);

  static void formatSize(uint32_t bytes, char* out, size_t outLen);
  static const char* basenameOf(const char* path);

};
