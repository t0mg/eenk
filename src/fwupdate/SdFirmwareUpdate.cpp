// eenk — SdFirmwareUpdate implementation.
// See SdFirmwareUpdate.h + src/fwupdate/README.md for provenance and the list
// of deliberate deviations from CrossPoint's SdFirmwareUpdateActivity.
#include "SdFirmwareUpdate.h"

#include <Arduino.h>
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>

#include <algorithm>
#include <cstring>

#include "FirmwareFlasher.h"
#include "HalInit.h"
#include "HalTypes.h"
#include "os/AppSettings.h"
#include "ui/SystemUI.h"

namespace {

void scanDirRecursive(const char* dirPath, int depth, std::vector<std::string>& out) {
  if (depth > SdFirmwareUpdate::kMaxScanDepth || out.size() >= SdFirmwareUpdate::kMaxEntries) return;

  File dir = SD_FS.open(dirPath);
  if (!dir || !dir.isDirectory()) return;

  File f = dir.openNextFile();
  while (f && out.size() < SdFirmwareUpdate::kMaxEntries) {
    String nameStr = f.name();
    const char* rawName = nameStr.c_str();
    const char* slash = strrchr(rawName, '/');
    const char* base = slash ? slash + 1 : rawName;

    char full[256];
    if (rawName[0] == '/') {
      snprintf(full, sizeof(full), "%s", rawName);
    } else {
      snprintf(full, sizeof(full), "%s/%s", dirPath, base);
    }

    if (f.isDirectory()) {
      if (base[0] != '.' && strcasecmp(base, "System Volume Information") != 0) {
        scanDirRecursive(full, depth + 1, out);
      }
    } else {
      if (base[0] != '.') {
        const size_t len = strlen(base);
        if (len > 4 && strcasecmp(base + len - 4, ".bin") == 0) {
          out.emplace_back(full);
        }
      }
    }
    f = dir.openNextFile();
  }
  dir.close();
}

struct ProgressCtx {
  SystemUI* ui;
  unsigned lastPct;
};

// firmware_flash::ProgressCb. Throttled to 5% steps: each update is an e-ink
// full refresh (~1s), so CrossPoint's per-percent cadence would add minutes.
void onFlashProgress(size_t written, size_t total, void* ctx) {
  auto* p = static_cast<ProgressCtx*>(ctx);
  const unsigned pct = total > 0 ? static_cast<unsigned>((written * 100) / total) : 0;
  if (pct < p->lastPct + 5 && pct != 100) return;
  p->lastPct = pct;
  p->ui->showLoading("Updating firmware...", pct / 100.0f);
}

void drainStaleInput(IInput& input, unsigned long ms) {
  const unsigned long until = millis() + ms;
  while (millis() < until) {
    input.pollInput();
    delay(10);
  }
}

// Power-button sleep from inside the updater. Recovery never returns to the
// caller, so without this a long-press would be swallowed by the modal
// pickers (which ignore SLEEP) and trap the user until the battery dies.
// Sleeping shows the standard cover, arms the power-button wakeup, and
// deep-sleeps; the next power-on (without UP held) boots normally.
[[noreturn]] void sleepNow(IDisplay& display) {
  SystemUI ui(display);
  ui.showSleepCover();
  delay(500);
  HalInit::prepareForSleep();
  esp_deep_sleep_start();
  for (;;) delay(1000);  // unreachable
}

// IInput decorator: deep-sleep on SLEEP instead of returning it, so the
// blocking Menu/Confirm modals (which ignore SLEEP) can't swallow a
// power-button long-press. All other events pass through untouched.
class SleepAwareInput : public IInput {
 public:
  SleepAwareInput(IInput& inner, IDisplay& display) : _inner(inner), _display(display) {}
  ButtonEvent pollInput() override {
    const ButtonEvent ev = _inner.pollInput();
    if (ev == ButtonEvent::SLEEP) sleepNow(_display);
    return ev;
  }
  bool getTouchPosition(int& x, int& y) const override { return _inner.getTouchPosition(x, y); }
  uint32_t getLastActivityTime() const override { return _inner.getLastActivityTime(); }
  void setAutoSleepTimeout(uint16_t seconds) override { _inner.setAutoSleepTimeout(seconds); }
  void resetActivityTimer() override { _inner.resetActivityTimer(); }

 private:
  IInput& _inner;
  IDisplay& _display;
};

}  // namespace

const char* SdFirmwareUpdate::basenameOf(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

void SdFirmwareUpdate::formatSize(uint32_t bytes, char* out, size_t outLen) {
  if (bytes < 1024) {
    snprintf(out, outLen, "%u B", (unsigned)bytes);
  } else if (bytes < 1024 * 1024) {
    snprintf(out, outLen, "%u KB", (unsigned)(bytes / 1024));
  } else {
    snprintf(out, outLen, "%.1f MB", bytes / (1024.0f * 1024.0f));
  }
}

void SdFirmwareUpdate::scanForFirmware(std::vector<std::string>& outPaths) {
  outPaths.clear();
  scanDirRecursive("/", 0, outPaths);
  std::sort(outPaths.begin(), outPaths.end());
}

bool SdFirmwareUpdate::validateFirmware(const std::string& path, std::string& outError) {
  File f = SD_FS.open(path.c_str(), FILE_READ);
  if (!f) {
    outError = "Cannot open file";
    return false;
  }
  const size_t fwSize = f.size();
  f.close();

  // Resolve the next-update partition directly (mirrors CrossPoint: no
  // Update.begin probe, which would erase partition state as a side effect).
  const esp_partition_t* dest = esp_ota_get_next_update_partition(nullptr);
  if (!dest) {
    Serial.println("[FW] no next-update partition available");
    outError = "Invalid firmware file";
    return false;
  }
  if (fwSize > dest->size) {
    Serial.printf("[FW] firmware (%u bytes) exceeds partition (%u bytes)\n", (unsigned)fwSize,
                  (unsigned)dest->size);
    outError = "Firmware too large for partition";
    return false;
  }

  const auto vr = firmware_flash::validateImageFile(path.c_str(), dest->size);
  if (vr != firmware_flash::Result::OK) {
    Serial.printf("[FW] image validation failed: %s\n", firmware_flash::resultName(vr));
    if (vr == firmware_flash::Result::TOO_LARGE) {
      outError = "Firmware too large for partition";
    } else if (vr == firmware_flash::Result::TOO_SMALL) {
      outError = "Firmware file is too small";
    } else if (vr == firmware_flash::Result::BAD_CHIP || vr == firmware_flash::Result::WRONG_BOARD) {
      outError = "Firmware is for a different device";
    } else {
      outError = "Invalid firmware file";
    }
    return false;
  }
  return true;
}

void SdFirmwareUpdate::waitForDismiss(IInput& input, IDisplay& display) {
  delay(300);
  drainStaleInput(input, 100);
  for (;;) {
    const ButtonEvent ev = input.pollInput();
    if (ev == ButtonEvent::SLEEP) sleepNow(display);
    if (ev == ButtonEvent::CONFIRM || ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) return;
    delay(50);
  }
}

void SdFirmwareUpdate::run(IDisplay& display, IInput& input, bool recoveryMode) {
  SystemUI ui(display);
  const char* title = recoveryMode ? "Recovery Mode" : "SD Card Firmware Update";
  // Route all input through the sleep-aware decorator so a power long-press
  // deep-sleeps even inside the blocking picker/confirm modals (which ignore
  // SLEEP). Recovery leaves via reboot or power-button sleep; a sleep followed
  // by a power-on without UP boots normally.
  SleepAwareInput sleepInput(input, display);

  // Never auto-sleep inside the updater (mirrors preventAutoSleep); restored
  // on every non-recovery exit below. Recovery only leaves via reboot/sleep.
  sleepInput.setAutoSleepTimeout(0);
  sleepInput.resetActivityTimer();
  drainStaleInput(sleepInput, 150);

  auto restoreSleep = [&input]() {
    input.setAutoSleepTimeout(AppSettings::load().sleepTimeoutSec);
    input.resetActivityTimer();
  };

  for (;;) {
    // ── PICK ──────────────────────────────────────────────────────────────
    std::vector<std::string> paths;
    scanForFirmware(paths);
    if (paths.empty()) {
      ui.showMessage(title, "No .bin files found.\n\nPlace a firmware .bin anywhere on the SD card.");
      waitForDismiss(sleepInput, display);
      if (!recoveryMode) {
        restoreSleep();
        return;
      }
      // The card may have been inserted after boot; re-mount before rescanning.
      StorageType remount;
      remount.begin();
      continue;
    }

    // Labels: "name.bin (3.3 MB)", disambiguated with the parent dir on
    // basename collisions.
    std::vector<std::string> labels;
    labels.reserve(paths.size());
    for (const auto& p : paths) {
      const char* base = basenameOf(p.c_str());
      int dupes = 0;
      for (const auto& q : paths) {
        if (&p != &q && strcmp(basenameOf(q.c_str()), base) == 0) dupes++;
      }
      std::string label = base;
      if (dupes > 0) {
        // Prefix the parent directory: "/a/fw.bin" + "/b/fw.bin" -> "a/fw.bin".
        const char* end = strrchr(p.c_str(), '/');
        if (end && end > p.c_str()) {
          const char* start = end - 1;
          while (start > p.c_str() && *(start - 1) != '/') start--;
          label = std::string(start, end) + "/" + base;
        }
      }
      File f = SD_FS.open(p.c_str(), FILE_READ);
      if (f) {
        char sizeBuf[16];
        formatSize((uint32_t)f.size(), sizeBuf, sizeof(sizeBuf));
        f.close();
        label += " (";
        label += sizeBuf;
        label += ")";
      }
      labels.push_back(label);
    }

    const int sel = ui.showMenuModal(sleepInput, "Select firmware", labels, 0, "eenk");
    if (sel < 0 || sel >= static_cast<int>(paths.size())) {
      if (recoveryMode) continue;  // cannot escape recovery (mirrors CrossPoint)
      restoreSleep();
      return;
    }
    const std::string& fwPath = paths[static_cast<size_t>(sel)];

    // ── VALIDATE ──────────────────────────────────────────────────────────
    ui.showLoading("Validating firmware...", 0.0f);
    std::string error;
    if (!validateFirmware(fwPath, error)) {
      ui.showMessage("Update failed", error.c_str());
      waitForDismiss(sleepInput, display);
      if (recoveryMode) continue;
      restoreSleep();
      return;
    }

    // ── CONFIRM ───────────────────────────────────────────────────────────
    std::string prompt(basenameOf(fwPath.c_str()));
    prompt += "\n\nDo not power off during the update.";
    if (!ui.showConfirmDialog(sleepInput, "Update firmware?", prompt.c_str(), "eenk")) {
      if (recoveryMode) continue;
      restoreSleep();
      return;
    }

    // ── UPDATE ────────────────────────────────────────────────────────────
    Serial.printf("[FW] SD update: %s\n", fwPath.c_str());
    ProgressCtx pctx{&ui, 0};
    ui.showLoading("Updating firmware...", 0.0f);
    // Re-validate at flash time (TOCTOU): SD is removable, so don't trust the
    // pre-confirmation pass (mirrors CrossPoint's performUpdate comment).
    const auto result = firmware_flash::flashFromSdPath(fwPath.c_str(), onFlashProgress, &pctx);
    if (result != firmware_flash::Result::OK) {
      Serial.printf("[FW] flash failed: %s\n", firmware_flash::resultName(result));
      const char* msg = (result == firmware_flash::Result::BAD_CHIP ||
                         result == firmware_flash::Result::WRONG_BOARD)
                            ? "Firmware is for a different device"
                            : "Firmware write failed";
      ui.showMessage("Update failed", msg);
      waitForDismiss(sleepInput, display);
      if (recoveryMode) continue;
      restoreSleep();
      return;
    }

    // ── SUCCESS ───────────────────────────────────────────────────────────
    Serial.println("[FW] SD firmware update complete, restarting");
    ui.showMessage("Update complete", "Restarting...");
    delay(1500);
    ESP.restart();
  }
}
