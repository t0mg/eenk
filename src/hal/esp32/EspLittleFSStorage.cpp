#ifdef PLATFORM_ESP32

#include "EspLittleFSStorage.h"
#include <Arduino.h>

extern const uint8_t intercept_start[] asm("_binary_data_the_intercept_bin_start");
extern const uint8_t intercept_end[]   asm("_binary_data_the_intercept_bin_end");

EspLittleFSStorage::EspLittleFSStorage()
{
}

EspLittleFSStorage::~EspLittleFSStorage()
{
}

const unsigned char* EspLittleFSStorage::readFileBinary(const char* filename, std::size_t* outSize)
{
    if (outSize) *outSize = 0;

    // We only support the_intercept.bin via embedded symbols for this Wokwi milestone
    if (strstr(filename, "the_intercept.bin") == nullptr) {
        Serial.printf("[EspLittleFSStorage] File not found: %s\n", filename);
        return nullptr;
    }

    size_t size = intercept_end - intercept_start;
    
    // Allocate RAM buffer (simulating a file read to accurately test memory budget)
    void* buffer = malloc(size);
    if (!buffer) {
        Serial.printf("[EspLittleFSStorage] Failed to allocate %zu bytes\n", size);
        return nullptr;
    }

    memcpy(buffer, intercept_start, size);

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
    return strstr(path, "the_intercept.bin") != nullptr;
}

#endif // PLATFORM_ESP32
