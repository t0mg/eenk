#pragma once
#include "HalTypes.h"

#if HAS_FLASH_CACHE

#include <cstddef>
#include <cstdint>
#include "esp_partition.h"

// Forward declarations for ESP-IDF types
typedef uint32_t esp_partition_mmap_handle_t;

class EspSdStorage; // Forward declaration

class FlashCache
{
public:
    FlashCache();
    ~FlashCache();

    /**
     * Load a story binary from SD card into the ink_cache flash partition,
     * then memory-map it for zero-copy access.
     *
     * @param sdStorage  The SD card storage to read from
     * @param sdPath     Path to the .bin file on SD card
     * @param outPtr     Receives the memory-mapped pointer to the story data
     * @param outSize    Receives the story binary size in bytes
     * @return true on success
     *
     * If the file is already cached (hash match), skips the SD->flash copy.
     * Caller should NOT free the returned pointer - it is memory-mapped.
     */
    bool loadStoryStreaming(EspSdStorage& sdStorage,
                            const char* sdPath,
                            const unsigned char** outPtr,
                            std::size_t* outSize);

    /** Unmap the current story from memory. Call before loading a new story. */
    void unload();

    /** Returns true if a story is currently memory-mapped. */
    bool isLoaded() const { return _mapped; }

    /** Progress callback for UI during flash writes. 0.0 to 1.0. */
    using ProgressCallback = void(*)(float progress, void* ctx);
    void setProgressCallback(ProgressCallback cb, void* ctx = nullptr) { _progressCb = cb; _progressCtx = ctx; }

    /** Check if the ink_cache partition exists. */
    bool findPartition();

    /** Get the CRC32 hash of the loaded story */
    uint32_t getHash() const { return _cachedHash; }

private:
    const esp_partition_t*        _partition = nullptr;
    esp_partition_mmap_handle_t   _mmapHandle = 0;
    const void*                   _mappedPtr = nullptr;
    bool                          _mapped = false;
    std::size_t                   _cachedSize = 0;
    uint32_t                      _cachedHash = 0;
    ProgressCallback              _progressCb = nullptr;
    void*                         _progressCtx = nullptr;

    bool mapFromFlash(std::size_t size);
    bool loadHashFromNvs(uint32_t* outHash, std::size_t* outSize);
    void saveHashToNvs(uint32_t hash, std::size_t size);
};

#endif // HAS_FLASH_CACHE
