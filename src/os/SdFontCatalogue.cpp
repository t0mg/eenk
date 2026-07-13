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

// Extract the font family stem from a filename.
// Accepts "<stem>-regular.epdfont" → returns "<stem>"
// Accepts "<stem>.epdfont"         → returns "<stem>"
// Other patterns return false (e.g. "-bold.epdfont" variants are skipped).
static bool extractStem(const char* filename, char* outStem, size_t outLen) {
    size_t len = strlen(filename);

    // Must end with .epdfont
    if (len < 8 || strcasecmp(filename + len - 8, ".epdfont") != 0) return false;
    size_t nameLen = len - 8; // strip .epdfont

    // Skip variant suffixes — these are sub-files of a family, not family roots.
    // Compare only within the bare stem region (not the .epdfont extension).
    static const char* const kVariantSuffixes[] = {
        "-bold", "-italic", "-bolditalic", nullptr
    };
    for (const char* const* s = kVariantSuffixes; *s; ++s) {
        size_t sLen = strlen(*s);
        if (nameLen >= sLen) {
            // Compare last sLen characters of the bare stem
            if (strncasecmp(filename + nameLen - sLen, *s, sLen) == 0) {
                return false; // This is a variant file, skip it
            }
        }
    }

    // Strip "-regular" suffix if present — the stem is the part before it.
    // Use strncasecmp to compare within the bare stem region (not the .epdfont extension).
    static const char kRegSuffix[] = "-regular";
    static const size_t kRegLen = sizeof(kRegSuffix) - 1;
    if (nameLen >= kRegLen && strncasecmp(filename + nameLen - kRegLen, kRegSuffix, kRegLen) == 0) {
        nameLen -= kRegLen;
    }

    if (nameLen == 0 || nameLen >= outLen) return false;
    memcpy(outStem, filename, nameLen);
    outStem[nameLen] = '\0';

    // Pretty-print: underscores/hyphens → spaces, capitalize first letter
    for (size_t i = 0; i < nameLen; i++) {
        if (outStem[i] == '_') outStem[i] = ' ';
        // Keep hyphens as-is for now (e.g. "sans-medium" → "sans-medium")
    }
    if (nameLen > 0) outStem[0] = (char)toupper((unsigned char)outStem[0]);
    return true;
}

// Build the raw stem (without pretty-print) for the FontEntry::stem field
static size_t rawStemLength(const char* filename) {
    size_t len = strlen(filename);
    if (len < 8) return 0;
    size_t nameLen = len - 8; // strip .epdfont
    static const char kRegSuffix[] = "-regular";
    static const size_t kRegLen = sizeof(kRegSuffix) - 1;
    // Check last kRegLen characters of the bare stem region (not the extension)
    if (nameLen >= kRegLen && strncasecmp(filename + nameLen - kRegLen, kRegSuffix, kRegLen) == 0)
        nameLen -= kRegLen;
    return nameLen;
}


void SdFontCatalogue::scan() {
    _count = 0;
    memset(_entries, 0, sizeof(_entries));

    // 1. Add built-in fonts
    for (size_t i = 0; i < kBuiltinFontCount && _count < MAX_FONTS; i++) {
        // Skip aliases: if the regular font pointer matches a previously added entry, it's an alias
        bool isAlias = false;
        for (size_t j = 0; j < i; j++) {
            if (kBuiltinFonts[i].regular == kBuiltinFonts[j].regular) {
                isAlias = true;
                break;
            }
        }
        if (isAlias) continue;

        FontEntry& e = _entries[_count++];
        strncpy(e.displayName, kBuiltinFonts[i].displayName, sizeof(e.displayName) - 1);
        e.stem[0]  = '\0';
        e.path[0]  = '\0';
        e.builtinIndex = (uint8_t)i;
    }

    // 2. Scan SD card /fonts directory for family roots
#ifdef PLATFORM_ESP32
    File dir = SD.open("/fonts");
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f && _count < MAX_FONTS) {
            if (!f.isDirectory()) {
                String name = f.name();
                char stem[32] = {};
                char displayName[32] = {};
                if (extractStem(name.c_str(), displayName, sizeof(displayName))) {
                    // Get raw stem (for loading)
                    size_t rawLen = rawStemLength(name.c_str());
                    if (rawLen > 0 && rawLen < sizeof(stem)) {
                        memcpy(stem, name.c_str(), rawLen);
                        stem[rawLen] = '\0';
                    }
                    FontEntry& e = _entries[_count++];
                    strncpy(e.displayName, displayName, sizeof(e.displayName) - 1);
                    strncpy(e.stem, stem,        sizeof(e.stem) - 1);
                    strncpy(e.path, "/fonts",    sizeof(e.path) - 1);
                    e.builtinIndex = 0;
                }
            }
            f = dir.openNextFile();
        }
        dir.close();
    }
#else
    DIR* dp = opendir("fonts");
    if (dp) {
        struct dirent* ep;
        while ((ep = readdir(dp)) != nullptr && _count < MAX_FONTS) {
            char displayName[32] = {};
            if (!extractStem(ep->d_name, displayName, sizeof(displayName))) continue;
            size_t rawLen = rawStemLength(ep->d_name);
            char stem[32] = {};
            if (rawLen > 0 && rawLen < sizeof(stem)) {
                memcpy(stem, ep->d_name, rawLen);
                stem[rawLen] = '\0';
            }
            FontEntry& e = _entries[_count++];
            strncpy(e.displayName, displayName, sizeof(e.displayName) - 1);
            strncpy(e.stem, stem,     sizeof(e.stem) - 1);
            strncpy(e.path, "fonts",  sizeof(e.path) - 1);
            e.builtinIndex = 0;
        }
        closedir(dp);
    }
#endif
}

