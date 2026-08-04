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

static const char* findLastSep(const char* str, size_t len) {
    const char* last = nullptr;
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == '/' || str[i] == '\\') {
            last = str + i;
        }
    }
    return last;
}

void StoryMetadata::getSavePath(const char* storyBinPath, char* outSavePath, size_t maxLen) {
    if (!storyBinPath || !outSavePath || maxLen == 0) return;

    size_t fullLen = strlen(storyBinPath);
    const char* lastSep = findLastSep(storyBinPath, fullLen);

    const char* baseName = lastSep ? lastSep + 1 : storyBinPath;

    // Fallback to bin filename (e.g. "my_story.bin") if no story folder exists
    const char* saveStem = baseName;
    char folderName[128] = {0};

    if (lastSep && lastSep > storyBinPath) {
        size_t dirLen = lastSep - storyBinPath;
        while (dirLen > 0 && (storyBinPath[dirLen - 1] == '/' || storyBinPath[dirLen - 1] == '\\')) {
            dirLen--;
        }

        if (dirLen > 0) {
            const char* prevSep = findLastSep(storyBinPath, dirLen);
            const char* folderStart = prevSep ? prevSep + 1 : storyBinPath;
            size_t folderLen = (storyBinPath + dirLen) - folderStart;

            if (folderLen > 0 && folderLen < sizeof(folderName)) {
                strncpy(folderName, folderStart, folderLen);
                folderName[folderLen] = '\0';

                if (strcasecmp(folderName, "stories") != 0) {
                    saveStem = folderName;
                }
            }
        }
    }

    snprintf(outSavePath, maxLen, "/.eenk_saves/%s.sav", saveStem);
}
