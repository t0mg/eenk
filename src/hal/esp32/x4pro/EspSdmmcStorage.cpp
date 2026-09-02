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

class EspSdmmcFileWriter : public IFileWriter {
    File& _f;
public:
    explicit EspSdmmcFileWriter(File& f) : _f(f) {}
    bool write(const void* data, std::size_t size) override {
        if (!_f) return false;
        return _f.write(static_cast<const uint8_t*>(data), size) == size;
    }
};

bool EspSdmmcStorage::writeStream(const char* path, const std::function<bool(IFileWriter&)>& writer) {
    if (!_initialized && !begin()) return false;
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return false;
    EspSdmmcFileWriter fw(f);
    bool ok = writer(fw);
    f.close();
    return ok;
}

class EspSdmmcFileReader : public IFileReader {
    File& _f;
public:
    explicit EspSdmmcFileReader(File& f) : _f(f) {}
    size_t read(void* dest, size_t size) override {
        if (!_f) return 0;
        return _f.read(static_cast<uint8_t*>(dest), size);
    }
    bool seek(size_t offset) override {
        if (!_f) return false;
        return _f.seek(offset);
    }
    size_t tell() const override {
        if (!_f) return 0;
        return _f.position();
    }
    size_t size() const override {
        if (!_f) return 0;
        return _f.size();
    }
};

bool EspSdmmcStorage::readStream(const char* path, const std::function<bool(IFileReader&)>& reader) {
    if (!_initialized && !begin()) return false;
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) return false;
    EspSdmmcFileReader fr(f);
    bool ok = reader(fr);
    f.close();
    return ok;
}

bool EspSdmmcStorage::renameFile(const char* oldPath, const char* newPath) {
    if (!_initialized && !begin()) return false;
    if (SD_MMC.exists(newPath)) {
        SD_MMC.remove(newPath);
    }
    return SD_MMC.rename(oldPath, newPath);
}

bool EspSdmmcStorage::writeFileBinary(const char* path, const unsigned char* data, std::size_t size) {
    if (!_initialized && !begin()) return false;
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write(data, size);
    f.close();
    return written == size;
}

bool EspSdmmcStorage::deleteFile(const char* path) {
    if (!_initialized && !begin()) return false;
    return SD_MMC.remove(path);
}

bool EspSdmmcStorage::fileExists(const char* path) {
    if (!_initialized && !begin()) return false;
    return SD_MMC.exists(path);
}

#endif
