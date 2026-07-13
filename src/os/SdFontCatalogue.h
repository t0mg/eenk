#pragma once
#include <cstdint>
#include <cstddef>
#include <BuiltinFonts.h>

struct FontEntry {
    char displayName[32];
    char stem[32];     // For SD fonts: the stem name used to load the family (e.g. "palatino")
    char path[128];    // Empty for built-ins; for SD fonts: directory that contains the family
    uint8_t builtinIndex; // Valid if path is empty
};

class SdFontCatalogue {
public:
    static constexpr size_t MAX_FONTS = 32;

    SdFontCatalogue();

    // Scans the /fonts directory on the SD card (or local fs on native)
    // and merges with kBuiltinFonts.
    // SD font detection: looks for <stem>-regular.epdfont or <stem>.epdfont.
    void scan();

    // Returns true if the entry at index is an SD font (path non-empty).
    bool isSD(size_t index) const {
        return index < _count && _entries[index].path[0] != '\0';
    }

    const FontEntry* getEntries() const { return _entries; }
    size_t getCount() const { return _count; }

private:
    FontEntry _entries[MAX_FONTS];
    size_t _count;
};
