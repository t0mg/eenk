#ifdef PLATFORM_ESP32

#include "EspSdmmcStorage.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

// SDMMC pin configuration for Xteink X4 Pro
#define SDMMC_PIN_CLK  41
#define SDMMC_PIN_CMD  42
#define SDMMC_PIN_D0   40
#define SDMMC_PIN_PWR  5   // Active-LOW power gate

static bool s_sdmmcMounted = false;

EspSdmmcStorage::EspSdmmcStorage() {}

EspSdmmcStorage::~EspSdmmcStorage() {}

bool EspSdmmcStorage::begin() {
    if (_initialized || s_sdmmcMounted) {
        _initialized = true;
        return true;
    }

    pinMode(SDMMC_PIN_PWR, OUTPUT);

    for (int retry = 0; retry < 3; ++retry) {
        // Power cycle SD card: HIGH (80ms) -> LOW (120ms)
        digitalWrite(SDMMC_PIN_PWR, HIGH);
        delay(80);
        digitalWrite(SDMMC_PIN_PWR, LOW);
        delay(120);

        if (!SD_MMC.setPins(SDMMC_PIN_CLK, SDMMC_PIN_CMD, SDMMC_PIN_D0, -1, -1, -1)) {
            Serial.println("[SDMMC] Failed to set SDMMC pins");
        }

        if (SD_MMC.begin("/sdcard", true /* 1-bit mode */, false /* format_if_mount_failed */, SDMMC_FREQ_DEFAULT, 5)) {
            Serial.println("[SDMMC] SD_MMC mounted successfully at /sdcard");
            _initialized = true;
            s_sdmmcMounted = true;
            return true;
        }

        Serial.printf("[SDMMC] SD_MMC mount attempt %d failed. Retrying...\n", retry + 1);
        SD_MMC.end();
    }

    Serial.println("[SDMMC] Failed to mount SD_MMC card after retries.");
    return false;
}

const unsigned char* EspSdmmcStorage::readFileBinary(const char* path, std::size_t* outSize) {
    if (outSize) *outSize = 0;
    if (!_initialized && !begin()) return nullptr;

    File f = SD_MMC.open(path, FILE_READ);
    if (!f) {
        Serial.printf("[SDMMC] Failed to open file: %s\n", path);
        return nullptr;
    }

    size_t size = f.size();
    if (size == 0) {
        f.close();
        return nullptr;
    }

    unsigned char* buf = (unsigned char*)malloc(size + 1);
    if (buf) {
        size_t readBytes = f.read(buf, size);
        f.close();
        buf[readBytes] = '\0';
        if (outSize) *outSize = readBytes;
        return buf;
    }

#if defined(BOARD_HAS_PSRAM)
    buf = (unsigned char*)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) {
        size_t readBytes = f.read(buf, size);
        f.close();
        buf[readBytes] = '\0';
        if (outSize) *outSize = readBytes;
        return buf;
    }
#endif

    f.close();
    return nullptr;
}

void EspSdmmcStorage::freeBuffer(const unsigned char* buf) {
    if (buf) {
        free((void*)buf);
    }
}

bool EspSdmmcStorage::fileExists(const char* path) {
    if (!_initialized && !begin()) return false;
    return SD_MMC.exists(path);
}

#endif
