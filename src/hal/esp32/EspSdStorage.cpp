#include "EspSdStorage.h"
#include <Arduino.h>
#include <SPI.h>
#include <cstdlib>

EspSdStorage::EspSdStorage()
{
}

EspSdStorage::~EspSdStorage()
{
}

bool EspSdStorage::begin()
{
    if (_initialized) return true;

    // The SPI bus is already initialized by EspEinkDisplay.
    // SD.begin() will use the existing SPI instance — we only pass the CS pin.
    // Do NOT call SPI.begin() here; it would reconfigure the bus and crash
    // because GPIO12 is also SPIHD on ESP32-C3.
    if (!SD.begin(SD_CS_PIN, SPI, 40000000)) {
        Serial.println("[SD] SD card init failed");
        return false;
    }

    Serial.println("[SD] SD card detected");
    _initialized = true;
    return true;
}

const unsigned char* EspSdStorage::readFileBinary(const char* path, std::size_t* outSize)
{
    if (!_initialized) return nullptr;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[SD] File not found: %s\n", path);
        return nullptr;
    }

    std::size_t size = file.size();
    unsigned char* buf = (unsigned char*)std::malloc(size);
    if (!buf) {
        Serial.printf("[SD] malloc failed for %u bytes\n", (unsigned)size);
        file.close();
        return nullptr;
    }

    size_t bytesRead = file.read(buf, size);
    file.close();

    if (bytesRead != size) {
        Serial.printf("[SD] Short read: got %u of %u bytes\n", (unsigned)bytesRead, (unsigned)size);
        std::free(buf);
        return nullptr;
    }

    if (outSize) {
        *outSize = size;
    }
    Serial.printf("[SD] Read %u bytes from %s\n", (unsigned)size, path);
    return buf;
}

void EspSdStorage::freeBuffer(const unsigned char* buf)
{
    if (buf) {
        std::free((void*)buf);
    }
}

bool EspSdStorage::fileExists(const char* path)
{
    if (!_initialized) return false;
    return SD.exists(path);
}
