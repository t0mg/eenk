#ifdef PLATFORM_ESP32

#include "EspLittleFSStorage.h"
#include <Arduino.h>
#include <LittleFS.h>

EspLittleFSStorage::EspLittleFSStorage()
{
    LittleFS.begin(true); // format on fail
}

EspLittleFSStorage::~EspLittleFSStorage()
{
}

const unsigned char* EspLittleFSStorage::readFileBinary(const char* filename, std::size_t* outSize)
{
    if (outSize) *outSize = 0;

    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("[LittleFS] File not found: %s\n", filename);
        return nullptr;
    }

    size_t size = file.size();
    void* buffer = malloc(size);
    if (!buffer) {
        Serial.printf("[LittleFS] Failed to allocate %zu bytes\n", size);
        file.close();
        return nullptr;
    }

    size_t readBytes = file.read((uint8_t*)buffer, size);
    file.close();

    if (readBytes != size) {
        Serial.printf("[LittleFS] Short read: %zu of %zu bytes\n", readBytes, size);
        free(buffer);
        return nullptr;
    }

    if (outSize) {
        *outSize = size;
    }
    return (const unsigned char*)buffer;
}

void EspLittleFSStorage::freeBuffer(const unsigned char* buffer)
{
    if (buffer) {
        free((void*)buffer);
    }
}

bool EspLittleFSStorage::fileExists(const char* path)
{
    return LittleFS.exists(path);
}

#endif // PLATFORM_ESP32
