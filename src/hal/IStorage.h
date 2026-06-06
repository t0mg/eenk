#pragma once
#include <cstddef>
/**
 * EENK — IStorage: Platform-agnostic storage interface
 *
 * Implemented by:
 *   SDLStorage      (PLATFORM_NATIVE)  — std::fopen / std::fread
 *   SDCardStorage   (PLATFORM_ESP32)   — SD card via SPI (M2)
 */

class IStorage
{
public:
    virtual ~IStorage() = default;

    /**
     * Read an entire file into a freshly allocated buffer.
     * @param path    File path (platform-specific separator)
     * @param outSize Written with the number of bytes read on success
     * @return        Heap-allocated buffer, or nullptr on failure.
     *                Caller must call freeBuffer() when done.
     */
    virtual const unsigned char* readFileBinary(const char* path, std::size_t* outSize) = 0;

    /**
     * Release a buffer previously returned by readFileBinary().
     * Safe to call with nullptr.
     */
    virtual void freeBuffer(const unsigned char* buf) = 0;

    /** Returns true if the file exists and is readable. */
    virtual bool fileExists(const char* path) = 0;
};
