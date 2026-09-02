#include "HalTypes.h"

#if HAS_FLASH_CACHE

#include "FlashCache.h"
#include "EspSdStorage.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <Arduino.h>
#include <rom/crc.h>
#include <algorithm>

FlashCache::FlashCache()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

FlashCache::~FlashCache()
{
    unload();
}

void FlashCache::unload()
{
    if (_mapped) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        esp_partition_munmap(_mmapHandle);
#else
        spi_flash_munmap(_mmapHandle);
#endif
        _mappedPtr = nullptr;
        _mmapHandle = 0;
        _mapped = false;
    }
}

bool FlashCache::findPartition()
{
    if (_partition) return true;
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ink_cache");
    return _partition != nullptr;
}

bool FlashCache::mapFromFlash(std::size_t size)
{
    if (!_partition) return false;
    esp_err_t err = esp_partition_mmap(
        _partition, 0, size,
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ESP_PARTITION_MMAP_DATA,
#else
        SPI_FLASH_MMAP_DATA,
#endif
        &_mappedPtr, &_mmapHandle);
    
    if (err == ESP_OK) {
        _mapped = true;
        return true;
    }
    return false;
}

bool FlashCache::loadHashFromNvs(uint32_t* outHash, std::size_t* outSize)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ink_cache", NVS_READONLY, &my_handle);
    if (err != ESP_OK) return false;

    err = nvs_get_u32(my_handle, "hash", outHash);
    if (err != ESP_OK) { nvs_close(my_handle); return false; }
    
    uint32_t size32 = 0;
    err = nvs_get_u32(my_handle, "size", &size32);
    if (err != ESP_OK) { nvs_close(my_handle); return false; }
    
    *outSize = size32;
    nvs_close(my_handle);
    return true;
}

void FlashCache::saveHashToNvs(uint32_t hash, std::size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ink_cache", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;

    nvs_set_u32(my_handle, "hash", hash);
    nvs_set_u32(my_handle, "size", (uint32_t)size);
    nvs_commit(my_handle);
    nvs_close(my_handle);
}

// Memory map alignment helper
static inline size_t alignUp(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
}

bool FlashCache::loadStoryStreaming(EspSdStorage& sdStorage,
                                    const char* sdPath,
                                    const unsigned char** outPtr,
                                    std::size_t* outSize)
{
    if (!findPartition()) {
        return false;
    }

    File file = SD.open(sdPath, FILE_READ);
    if (!file) {
        Serial.printf("[FlashCache] Failed to open %s\n", sdPath);
        return false;
    }

    size_t fileSize = file.size();
    if (fileSize < 128) {
        Serial.printf("[FlashCache] File %s too small (%u bytes)\n", sdPath, (unsigned)fileSize);
        file.close();
        return false;
    }
    
    // First pass: Compute CRC32 over the file
    uint32_t fileCrc = 0;
    uint8_t buf[4096];
    size_t offset = 0;
    
    if (_progressCb) _progressCb(0.0f, _progressCtx);
    
    while (offset < fileSize) {
        size_t toRead = std::min(sizeof(buf), fileSize - offset);
        size_t n = file.read(buf, toRead);
        if (n != toRead) {
            Serial.printf("[FlashCache] CRC pass read error at offset %u (expected %u, got %u)\n",
                          (unsigned)offset, (unsigned)toRead, (unsigned)n);
            file.close();
            return false;
        }
        fileCrc = crc32_le(fileCrc, buf, toRead);
        offset += toRead;
    }
    
    file.close();
    
    uint32_t cachedHash = 0;
    std::size_t cachedSize = 0;
    bool hashMatches = false;
    
    if (loadHashFromNvs(&cachedHash, &cachedSize)) {
        if (cachedHash == fileCrc && cachedSize == fileSize) {
            hashMatches = true;
        }
    }
    
    if (!hashMatches) {
        // Ensure any existing mapping is released before modifying flash partition
        unload();

        file = SD.open(sdPath, FILE_READ);
        if (!file) {
            Serial.printf("[FlashCache] Failed to reopen %s for flashing\n", sdPath);
            return false;
        }

        // Need to write to flash
        esp_err_t err = esp_partition_erase_range(_partition, 0, alignUp(fileSize, 4096));
        if (err != ESP_OK) {
            Serial.printf("[FlashCache] Failed to erase partition range: %d\n", err);
            file.close();
            return false;
        }
        
        offset = 0;
        while (offset < fileSize) {
            size_t toRead = std::min(sizeof(buf), fileSize - offset);
            size_t n = file.read(buf, toRead);
            if (n != toRead) {
                Serial.printf("[FlashCache] Write pass read error at offset %u (expected %u, got %u)\n",
                              (unsigned)offset, (unsigned)toRead, (unsigned)n);
                file.close();
                return false;
            }
            
            // Pad to 4-byte boundary for esp_partition_write
            size_t toWrite = (toRead + 3) & ~3;
            if (toWrite > toRead) {
                memset(buf + toRead, 0xFF, toWrite - toRead);
            }
            
            err = esp_partition_write(_partition, offset, buf, toWrite);
            if (err != ESP_OK) {
                Serial.printf("[FlashCache] Failed to write partition at offset %u: %d\n", (unsigned)offset, err);
                file.close();
                return false;
            }
            
            offset += toRead;
            if (_progressCb) {
                _progressCb((float)offset / fileSize, _progressCtx);
            }
        }
        file.close();
        
        // Verify header written in flash
        uint8_t verifyHeader[128];
        err = esp_partition_read(_partition, 0, verifyHeader, sizeof(verifyHeader));
        if (err != ESP_OK || memcmp(verifyHeader, "eenk", 4) != 0) {
            Serial.printf("[FlashCache] Flash write verification failed! First 4 bytes in flash: %02X %02X %02X %02X\n",
                          verifyHeader[0], verifyHeader[1], verifyHeader[2], verifyHeader[3]);
            return false;
        }

        saveHashToNvs(fileCrc, fileSize);
    } else {
        if (_progressCb) _progressCb(1.0f, _progressCtx);
    }
    
    // Ensure any previously mapped file is unmapped
    unload();
    
    if (mapFromFlash(fileSize)) {
        *outPtr = (const unsigned char*)_mappedPtr;
        *outSize = fileSize;
        return true;
    }
    
    return false;
}

#endif // HAS_FLASH_CACHE
