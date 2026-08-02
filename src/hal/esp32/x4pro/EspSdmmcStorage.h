#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IStorage.h"
#include <cstddef>
#include <FS.h>
#include <SD_MMC.h>

class EspSdmmcStorage : public IStorage
{
public:
    EspSdmmcStorage();
    ~EspSdmmcStorage() override;

    bool begin();

    const unsigned char* readFileBinary(const char* path, std::size_t* outSize) override;
    void freeBuffer(const unsigned char* buf) override;
    bool fileExists(const char* path) override;

private:
    bool _initialized = false;
};

#endif
