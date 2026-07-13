// EENK — BootManager implementation
//
// Boot mode and story path are stored in RTC memory (RTC_DATA_ATTR).
// RTC memory survives a software restart (ESP.restart()) but is cleared on
// power-off / hard reset — so the device always defaults to MENU mode on a
// cold boot, which is exactly what we want.
//
// This approach mirrors Papyrix and avoids all NVS NOT_FOUND / corruption
// issues that arise from using Preferences for transient reboot flags.
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

// RTC_NOINIT_ATTR places variables in .rtc.noinit which is NEVER re-copied from
// flash at startup. This means values survive ESP.restart() (soft reset) but are
// garbage on a cold power-on — hence the magic-number guard below.
// Contrast with RTC_DATA_ATTR (.rtc.data) which IS re-initialised from flash on
// every non-deep-sleep reset, making it useless for cross-restart state.

static constexpr uint32_t kRtcMagic = 0xB007BADE;  // "BOOT BADE" — same as Papyrix

RTC_NOINIT_ATTR static uint32_t rtcMagic;
RTC_NOINIT_ATTR static uint8_t  rtcBootMode;
RTC_NOINIT_ATTR static char     rtcStoryPath[128];

// ─── init() ──────────────────────────────────────────────────────────────────

void BootManager::init() {
    // On first cold boot rtcMagic is 0 (RTC uninitialised).
    // Validate magic; if missing, reset everything to safe defaults.
    if (rtcMagic != kRtcMagic) {
        rtcMagic        = kRtcMagic;
        rtcBootMode     = static_cast<uint8_t>(BootMode::MENU);
        rtcStoryPath[0] = '\0';
        Serial.println("[BootManager] RTC cold boot — defaulting to MENU");
    } else {
        Serial.printf("[BootManager] RTC warm boot — mode=%d\n", (int)rtcBootMode);
    }
}

// ─── getBootMode() ───────────────────────────────────────────────────────────

BootMode BootManager::getBootMode() {
    return static_cast<BootMode>(rtcBootMode);
}

// ─── setBootMode() ───────────────────────────────────────────────────────────

void BootManager::setBootMode(BootMode mode) {
    rtcBootMode = static_cast<uint8_t>(mode);
    Serial.printf("[BootManager] Boot mode set to %d\n", (int)mode);
}

// ─── setStoryPath() ──────────────────────────────────────────────────────────

void BootManager::setStoryPath(const char* path) {
    if (!path) return;
    strncpy(rtcStoryPath, path, sizeof(rtcStoryPath) - 1);
    rtcStoryPath[sizeof(rtcStoryPath) - 1] = '\0';
}

// ─── getStoryPath() ──────────────────────────────────────────────────────────

bool BootManager::getStoryPath(char* outPath, size_t maxLen) {
    if (!outPath || maxLen == 0 || rtcStoryPath[0] == '\0') return false;
    strncpy(outPath, rtcStoryPath, maxLen);
    outPath[maxLen - 1] = '\0';
    return true;
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
