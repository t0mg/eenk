#pragma once

#include "BookStorage.h" // Assume this is where freeink::book::BookSource is
#include <cstdint>

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

class SdBookSource : public freeink::book::BookSource {
public:
    SdBookSource() = default;
    ~SdBookSource() { close(); }

    bool open(const char* path);
    void close();

    int32_t readAt(uint64_t offset, void* dst, uint32_t len) override;
    uint64_t size() const override;

private:
#ifdef PLATFORM_ESP32
    File _file;
#else
    FILE* _file = nullptr;
#endif
    uint64_t _size = 0;
};

} // namespace book
} // namespace eenk
