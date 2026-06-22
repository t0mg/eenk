// EENK — StoryMetadata implementation
// readFromSD reads the first 128 bytes of a .bin file from the SD card and
// validates the EENK header magic + version.
#include "StoryMetadata.h"

#ifdef PLATFORM_ESP32
#include <SD.h>

bool StoryMetadata::readFromSD(const char* path, StoryMetadata* out) {
    File f = SD.open(path, FILE_READ);
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
