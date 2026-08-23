#pragma once

#include "BookStorage.h"
#include <cstdint>
#include <cstddef>

#ifdef PLATFORM_ESP32
#include <FS.h>
#ifdef USE_SD_MMC
#include <SD_MMC.h>
#define SD_FS SD_MMC
#else
#include <SD.h>
#define SD_FS SD
#endif
#else
#include <cstdio>
#endif

namespace eenk {
namespace book {

class SdCacheStorage : public freeink::book::CacheStorage {
public:
    SdCacheStorage() = default;
    ~SdCacheStorage() { endWrite(); }

    void setBaseDir(const char* dir);

    bool exists(const char* name) override;
    bool remove(const char* name) override;
    int64_t fileSize(const char* name) override;
    int32_t readAt(const char* name, uint32_t offset, void* dst, uint32_t len) override;
    
    bool beginWrite(const char* name) override;
    bool write(const void* data, uint32_t len) override;
    bool endWrite() override;
    int32_t readBackAt(uint32_t offset, void* dst, uint32_t len) override;

private:
    char _baseDir[128] = {};
    char _writeName[64] = {};
    char _writeTmpPath[196] = {};
    char _writeFinalPath[196] = {};

#ifdef PLATFORM_ESP32
    File _writeFile;
#else
    FILE* _writeFile = nullptr;
#endif

    void buildPath(const char* name, char* buf, size_t cap) const;
};

} // namespace book
} // namespace eenk
