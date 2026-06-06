#pragma once

#include "hal/IStorage.h"

/**
 * EENK — EspLittleFSStorage
 *
 * Implements IStorage using ESP32 LittleFS.
 */
class EspLittleFSStorage : public IStorage
{
public:
    EspLittleFSStorage();
    virtual ~EspLittleFSStorage();

    const unsigned char* readFileBinary(const char* filename, std::size_t* outSize) override;
    void freeBuffer(const unsigned char* buffer) override;
    bool fileExists(const char* path) override;
};
