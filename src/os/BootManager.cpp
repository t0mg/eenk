// eenk — BootManager implementation
//
// Boot mode and story path are stored in NVS namespace "boot".
// To mirror Papyrix behavior, on a cold boot (power-on, external reset button)
// we reset to MENU mode, but on a warm boot (ESP.restart() soft reset) or
// deep sleep wake, we preserve the mode.
//
// AppSettings (fonts, margins, sleep timeout) are still persisted to NVS
// because they DO need to survive power-off.
//
// On PLATFORM_NATIVE all operations are stubs; getBootMode() always returns
// BootMode::MENU so the desktop simulation always goes to the menu.
#include "BootManager.h"

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include <esp_ota_ops.h>

/*
 * NOTE ON STORAGE STRATEGY (NVS vs RTC Memory):
 * We strictly use NVS (Preferences API) to persist the BootMode and story path.
 *
 * It is tempting to use RTC memory (e.g. RTC_NOINIT_ATTR) to avoid flash wear,
 * however the eenk hardware utilizes a latching power circuit to maximize
 * battery life. When the device sleeps on battery, it physically cuts power to
 * the ESP32, which completely zeroes out the RTC memory domain. Waking it up
 * triggers a cold power-on reset (esp_reset_reason == 1), NOT a deep sleep
 * wake-up.
 *
 * BEWARE: USB debugging is deceitful! When plugged into USB, the bus controller
 * keeps the ESP32 partially powered, allowing RTC memory to survive and making
 * it appear as a true deep sleep (esp_reset_reason == 8). Always test
 * sleep/wake functionality purely on battery power!
 */

// ─── init() ──────────────────────────────────────────────────────────────────

void BootManager::init() {
  Serial.println("[BootManager] Initialised (using NVS)");
}

// ─── getBootMode() ───────────────────────────────────────────────────────────

BootMode BootManager::getBootMode() {
  Preferences prefs;
  uint8_t mode = static_cast<uint8_t>(BootMode::MENU);
  if (prefs.begin("boot", false)) {
    mode = prefs.getUChar("mode", mode);
    prefs.end();
  }
  return static_cast<BootMode>(mode);
}

// ─── setBootMode() ───────────────────────────────────────────────────────────

void BootManager::setBootMode(BootMode mode) {
  Preferences prefs;
  if (prefs.begin("boot", false)) {
    prefs.putUChar("mode", static_cast<uint8_t>(mode));
    prefs.end();
    Serial.printf("[BootManager] Boot mode set to %d\n", (int)mode);
  }
}

// ─── setStoryPath() ──────────────────────────────────────────────────────────

void BootManager::setStoryPath(const char *path) {
  if (!path)
    return;
  Preferences prefs;
  if (prefs.begin("boot", false)) {
    prefs.putString("path", path);
    prefs.end();
  }
}

// ─── getStoryPath() ──────────────────────────────────────────────────────────

bool BootManager::getStoryPath(char *outPath, size_t maxLen) {
  if (!outPath || maxLen == 0)
    return false;
  Preferences prefs;
  if (prefs.begin("boot", true)) {
    String path = prefs.getString("path", "");
    prefs.end();
    if (path.length() > 0 && path.length() < maxLen) {
      strncpy(outPath, path.c_str(), maxLen);
      outPath[maxLen - 1] = '\0';
      return true;
    }
  }
  return false;
}

// ─── reboot() ────────────────────────────────────────────────────────────────

void BootManager::reboot() {
  // Flush serial so all pending log messages are visible on the monitor.
  Serial.flush();
  delay(200);
  ESP.restart();
}

// ─── bootUpdater() ───────────────────────────────────────────────────────────

void BootManager::bootUpdater() {
  Serial.println("[BootManager] Booting to OTA Updater (app1)...");
  const esp_partition_t *app1_part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  if (app1_part) {
    esp_ota_set_boot_partition(app1_part);
    reboot();
  } else {
    Serial.println("[BootManager] ERROR: OTA_1 partition not found!");
  }
}

// =============================================================================
#else // PLATFORM_NATIVE stubs
// =============================================================================

#include <cstdlib>
#include <cstring>

void BootManager::init() {}

BootMode BootManager::getBootMode() { return BootMode::MENU; }

void BootManager::setBootMode(BootMode /*mode*/) {}

static char nativeStoryPath[256] = "";

void BootManager::setStoryPath(const char * path) {
  if (path) {
    strncpy(nativeStoryPath, path, sizeof(nativeStoryPath) - 1);
    nativeStoryPath[sizeof(nativeStoryPath) - 1] = '\0';
  }
}

bool BootManager::getStoryPath(char *outPath, size_t maxLen) {
  if (nativeStoryPath[0] != '\0' && outPath && maxLen > 0) {
    strncpy(outPath, nativeStoryPath, maxLen);
    outPath[maxLen - 1] = '\0';
    return true;
  }
  return false;
}

void BootManager::reboot() { exit(0); }

void BootManager::bootUpdater() { exit(0); }

#endif // PLATFORM_ESP32
