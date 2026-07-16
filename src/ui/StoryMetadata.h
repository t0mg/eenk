// eenk — StoryMetadata
// 128-byte header prepended to ink .bin files.
// Allows the GameLibrary to display rich metadata (title, author) without
// loading the full story into memory.
#pragma once
#include <cstdint>
#include <cstring>

// eenk binary header prepended to ink .bin files.
// Total size: 128 bytes.
struct StoryMetadata {
    static constexpr uint32_t MAGIC   = 0x6B6E6565; // "eenk" little-endian
    static constexpr uint16_t VERSION = 1;
    static constexpr size_t   SIZE    = 128;

    uint32_t magic;          // 0x6B6E6565                          offset   0
    uint16_t version;        // 1                                   offset   4
    uint16_t headerSize;     // sizeof this struct = 128            offset   6
    char     title[64];      // null-terminated UTF-8 title         offset   8
    char     author[32];     // null-terminated UTF-8 author        offset  72
    uint32_t compileTime;    // Unix timestamp                      offset 104
    uint32_t flags;          // bit 0: has_media_sidecar            offset 108

    // Font hint — repurposed from formerly-reserved[16] (offset 112–127).
    //
    // fontNameLen == 0  → no font hint; runtime uses the user's setting.
    // fontNameLen  > 0  → fontName holds the hint (fontNameLen bytes, null-padded to 15).
    //
    // fontName is a plain name — no trailing-dot encoding needed.
    // Resolution order (in InkEngine):
    //   1. Try builtin token lookup (case-insensitive) — e.g. "sans", "sans-medium"
    //   2. Try SD card font family by stem — sidecar /eenk/<story>/ then /fonts/
    //   3. Fall back to user's device setting
    uint8_t  fontNameLen;    // 0 = no hint; >0 = valid byte count  offset 112
    char     fontName[15];   // null-padded font stem                offset 113

    // Copy the stem into buf[bufLen]. Returns false if no hint or buf too small.
    bool getFontStem(char* buf, size_t bufLen) const {
        if (fontNameLen == 0 || bufLen == 0) return false;
        uint8_t len = fontNameLen < 15 ? fontNameLen : 15;
        if (len >= bufLen) return false;
        memcpy(buf, fontName, len);
        buf[len] = '\0';
        return true;
    }


    // Parse a metadata header from the start of a .bin buffer.
    // Returns true if the header is valid (magic + version match).
    static bool parse(const uint8_t* buf, size_t bufLen, StoryMetadata* out) {
        if (bufLen < SIZE) return false;
        memcpy(out, buf, SIZE);
        return out->magic == MAGIC && out->version == VERSION;
    }

    // Check if a buffer starts with a valid eenk header without copying.
    static bool hasHeader(const uint8_t* buf, size_t bufLen) {
        if (bufLen < 8) return false;
        uint32_t m; memcpy(&m, buf, 4);
        uint16_t v; memcpy(&v, buf + 4, 2);
        return m == MAGIC && v == VERSION;
    }

    // Read metadata from an SD file without loading the whole file.
    // path: SD card path like "/eenk/story.bin"
    // Returns true on success.
    static bool readFromSD(const char* path, StoryMetadata* out);
};

static_assert(sizeof(StoryMetadata) == StoryMetadata::SIZE,
              "StoryMetadata must be exactly 128 bytes");
