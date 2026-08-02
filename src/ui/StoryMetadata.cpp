// eenk — StoryMetadata implementation
// readFromSD reads the first 128 bytes of a .bin file from the SD card and
// validates the eenk header magic + version.
#include "StoryMetadata.h"
#include <cstdio>
#include <cstring>

#ifdef PLATFORM_ESP32
#include "HalTypes.h"

bool StoryMetadata::readFromSD(const char* path, StoryMetadata* out) {
    File f = SD_FS.open(path, FILE_READ);
    if (!f) return false;
    uint8_t buf[SIZE];
    size_t n = f.read(buf, SIZE);
    f.close();
    if (n < SIZE) return false;
    return parse(buf, n, out);
}

#else
// Native/unit-test stub: not used during SDL simulation.
bool StoryMetadata::readFromSD(const char* /*path*/, StoryMetadata* /*out*/) {
    return false;
}
#endif

void StoryMetadata::getSavePath(const char* storyBinPath, char* outSavePath, size_t maxLen) {
    const char* lastSlash = strrchr(storyBinPath, '/');
    const char* baseName = lastSlash ? lastSlash + 1 : storyBinPath;

    char stem[128];
    strncpy(stem, baseName, sizeof(stem) - 1);
    stem[sizeof(stem) - 1] = '\0';
    char* dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    snprintf(outSavePath, maxLen, "/.eenk_saves/%s.sav", stem);
}
