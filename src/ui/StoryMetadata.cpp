// eenk — StoryMetadata implementation
// readFromSD reads the first 128 bytes of a .bin file from the SD card and
// validates the eenk header magic + version.
#include "StoryMetadata.h"
#include <cstdio>
#include <cstring>

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

void StoryMetadata::getSavePath(const char* storyPath, char* outSavePath, size_t maxLen) {
    if (!storyPath || !storyPath[0] || !outSavePath || maxLen == 0) return;

    const char* rel = storyPath;
    if (strncmp(rel, "/sd", 3) == 0) rel += 3;
    if (strncmp(rel, "/stories", 8) == 0) rel += 8;
    else if (strncmp(rel, "/eenk", 5) == 0) rel += 5;
    else if (strncmp(rel, "stories", 7) == 0) rel += 7;
    while (*rel == '/' || *rel == '\\') rel++;

    char sanitized[128] = {};
    size_t i = 0;
    for (; rel[i] != '\0' && i < sizeof(sanitized) - 1; ++i) {
        if (rel[i] == '/' || rel[i] == '\\') {
            sanitized[i] = '_';
        } else {
            sanitized[i] = rel[i];
        }
    }
    sanitized[i] = '\0';

    snprintf(outSavePath, maxLen, "/.eenk_saves/%s.save", sanitized);
}
