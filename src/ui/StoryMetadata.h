// EENK — StoryMetadata
// 128-byte header prepended to ink .bin files.
// Allows the GameLibrary to display rich metadata (title, author) without
// loading the full story into memory.
#pragma once
#include <cstdint>
#include <cstring>

// EENK binary header prepended to ink .bin files.
// Total size: 128 bytes.
struct StoryMetadata {
    static constexpr uint32_t MAGIC   = 0x4B4E4545; // "EENK" little-endian
    static constexpr uint16_t VERSION = 1;
    static constexpr size_t   SIZE    = 128;

    uint32_t magic;          // 0x4B4E4545
    uint16_t version;        // 1
    uint16_t headerSize;     // sizeof this struct = 128
    char     title[64];      // null-terminated UTF-8 title
    char     author[32];     // null-terminated UTF-8 author
    uint32_t compileTime;    // Unix timestamp
    uint32_t flags;          // reserved
    uint8_t  reserved[16];   // reserved, zero

    // Parse a metadata header from the start of a .bin buffer.
    // Returns true if the header is valid (magic + version match).
    static bool parse(const uint8_t* buf, size_t bufLen, StoryMetadata* out) {
        if (bufLen < SIZE) return false;
        memcpy(out, buf, SIZE);
        return out->magic == MAGIC && out->version == VERSION;
    }

    // Check if a buffer starts with a valid EENK header without copying.
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
