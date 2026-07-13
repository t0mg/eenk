#include "SdFontCatalogue.h"
#include <cstring>
#include <cctype>

#ifdef PLATFORM_ESP32
#include <SD.h>
#else
#include <dirent.h>
#endif

extern const BuiltinFontEntry kBuiltinFonts[];
extern const size_t kBuiltinFontCount;

SdFontCatalogue::SdFontCatalogue() : _count(0) {
    memset(_entries, 0, sizeof(_entries));
}

void SdFontCatalogue::titleFromFilename(const char *filename, char *outTitle, size_t outLen) {
    if (outLen == 0) return;

    size_t len = strlen(filename);
    if (len > 8) {
        const char *ext = filename + len - 8;
        if (strcasecmp(ext, ".epdfont") == 0) {
            len -= 8;
        }
    }
    size_t copyLen = (len < outLen - 1) ? len : outLen - 1;
    strncpy(outTitle, filename, copyLen);
    outTitle[copyLen] = '\0';

    for (size_t i = 0; i < copyLen; i++) {
        if (outTitle[i] == '_' || outTitle[i] == '-') {
            outTitle[i] = ' ';
        }
    }
    if (copyLen > 0) {
        outTitle[0] = (char)toupper((unsigned char)outTitle[0]);
    }
}

void SdFontCatalogue::scan() {
    _count = 0;
    memset(_entries, 0, sizeof(_entries));

    // 1. Add built-in fonts
    for (size_t i = 0; i < kBuiltinFontCount && _count < MAX_FONTS; i++) {
        FontEntry& e = _entries[_count++];
        strncpy(e.displayName, kBuiltinFonts[i].displayName, sizeof(e.displayName) - 1);
        e.path[0] = '\0';
        e.builtinIndex = (uint8_t)i;
    }

    // 2. Scan SD card
#ifdef PLATFORM_ESP32
    File dir = SD.open("/fonts");
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f && _count < MAX_FONTS) {
            if (!f.isDirectory()) {
                String name = f.name();
                if (name.endsWith(".epdfont") || name.endsWith(".EPDFONT")) {
                    FontEntry& e = _entries[_count++];
                    snprintf(e.path, sizeof(e.path), "/fonts/%s", name.c_str());
                    titleFromFilename(name.c_str(), e.displayName, sizeof(e.displayName));
                }
            }
            f = dir.openNextFile();
        }
        dir.close();
    }
#else
    DIR *dp = opendir("fonts");
    if (dp) {
        struct dirent *ep;
        while ((ep = readdir(dp)) != nullptr && _count < MAX_FONTS) {
            size_t len = strlen(ep->d_name);
            if (len > 8 && strcasecmp(ep->d_name + len - 8, ".epdfont") == 0) {
                FontEntry& e = _entries[_count++];
                snprintf(e.path, sizeof(e.path), "fonts/%s", ep->d_name);
                titleFromFilename(ep->d_name, e.displayName, sizeof(e.displayName));
            }
        }
        closedir(dp);
    }
#endif
}
