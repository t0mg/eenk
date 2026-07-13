#pragma once
#include <cstdint>
#include <cstddef>
#include <BuiltinFonts.h>

struct FontEntry {
    char displayName[32];
    char path[128]; // Empty for built-ins
    uint8_t builtinIndex; // Valid if path is empty
};

class SdFontCatalogue {
public:
    static constexpr size_t MAX_FONTS = 32;

    SdFontCatalogue();

    // Scans the /fonts directory on the SD card (or local fs on native)
    // and merges with kBuiltinFonts.
    void scan();

    const FontEntry* getEntries() const { return _entries; }
    size_t getCount() const { return _count; }

private:
    void titleFromFilename(const char *filename, char *outTitle, size_t outLen);

    FontEntry _entries[MAX_FONTS];
    size_t _count;
};
