// EENK — BootManager implementation
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
#include <cstring>
#include <esp_ota_ops.h>
#include <Preferences.h>

// ─── init() ──────────────────────────────────────────────────────────────────

void BootManager::init() {
    Serial.println("[BootManager] Initialised");
}

// ─── getBootMode() ───────────────────────────────────────────────────────────

BootMode BootManager::getBootMode() {
    Preferences prefs;
    uint8_t mode = static_cast<uint8_t>(BootMode::MENU);
    if (prefs.begin("boot", false)) {
        mode = prefs.getUChar("mode", mode);

        // Removed cold boot check to ensure we always resume the story.
        // esp_reset_reason_t reason = esp_reset_reason();
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

void BootManager::setStoryPath(const char* path) {
    if (!path) return;
    Preferences prefs;
    if (prefs.begin("boot", false)) {
        prefs.putString("path", path);
        prefs.end();
    }
}

// ─── getStoryPath() ──────────────────────────────────────────────────────────

bool BootManager::getStoryPath(char* outPath, size_t maxLen) {
    if (!outPath || maxLen == 0) return false;
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
    const esp_partition_t* app1_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (app1_part) {
        esp_ota_set_boot_partition(app1_part);
        reboot();
    } else {
        Serial.println("[BootManager] ERROR: OTA_1 partition not found!");
    }
}

// =============================================================================
#else  // PLATFORM_NATIVE stubs
// =============================================================================

#include <cstdlib>
#include <cstring>

void BootManager::init() {}

BootMode BootManager::getBootMode() {
    return BootMode::MENU;
}

void BootManager::setBootMode(BootMode /*mode*/) {}

void BootManager::setStoryPath(const char* /*path*/) {}

bool BootManager::getStoryPath(char* /*outPath*/, size_t /*maxLen*/) {
    return false;
}

void BootManager::reboot() {
    exit(0);
}

void BootManager::bootUpdater() {
    exit(0);
}

#endif  // PLATFORM_ESP32
