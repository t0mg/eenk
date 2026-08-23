#include "SdCacheStorage.h"
#include <cstring>
#include <sys/stat.h>

#ifndef PLATFORM_ESP32
#include <unistd.h>
#endif

namespace eenk {
namespace book {

void SdCacheStorage::buildPath(const char* name, char* buf, size_t cap) const {
    snprintf(buf, cap, "%s/%s", _baseDir, name);
}

void SdCacheStorage::setBaseDir(const char* dir) {
    strncpy(_baseDir, dir, sizeof(_baseDir) - 1);
    _baseDir[sizeof(_baseDir) - 1] = '\0';
    
    // Strip trailing slashes
    size_t len = strlen(_baseDir);
    while (len > 0 && (_baseDir[len - 1] == '/' || _baseDir[len - 1] == '\\')) {
        _baseDir[--len] = '\0';
    }

    // Create base dir if it doesn't exist
#ifdef PLATFORM_ESP32
    if (!SD_FS.exists("/.eenk_cache")) {
        SD_FS.mkdir("/.eenk_cache");
    }
    if (!SD_FS.exists(_baseDir)) {
        SD_FS.mkdir(_baseDir);
    }
#else
#ifdef _WIN32
    mkdir(".eenk_cache");
    mkdir(_baseDir);
#else
    mkdir(".eenk_cache", 0755);
    mkdir(_baseDir, 0755);
#endif
#endif
}

bool SdCacheStorage::exists(const char* name) {
    char path[196];
    buildPath(name, path, sizeof(path));
    
#ifdef PLATFORM_ESP32
    return SD_FS.exists(path);
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

bool SdCacheStorage::remove(const char* name) {
    char path[196];
    buildPath(name, path, sizeof(path));
    
#ifdef PLATFORM_ESP32
    return SD_FS.remove(path);
#else
    return std::remove(path) == 0;
#endif
}

int64_t SdCacheStorage::fileSize(const char* name) {
    char path[196];
    buildPath(name, path, sizeof(path));
    
#ifdef PLATFORM_ESP32
    if (!SD_FS.exists(path)) return -1;
    File f = SD_FS.open(path, FILE_READ);
    if (!f) return -1;
    int64_t s = f.size();
    f.close();
    return s;
#else
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
#endif
}

int32_t SdCacheStorage::readAt(const char* name, uint32_t offset, void* dst, uint32_t len) {
    char path[196];
    buildPath(name, path, sizeof(path));

#ifdef PLATFORM_ESP32
    File f = SD_FS.open(path, FILE_READ);
    if (!f) return -1;
    if (!f.seek(offset)) {
        f.close();
        return -1;
    }
    int32_t readBytes = f.read(static_cast<uint8_t*>(dst), len);
    f.close();
    return readBytes;
#else
    FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    if (std::fseek(f, offset, SEEK_SET) != 0) {
        std::fclose(f);
        return -1;
    }
    int32_t readBytes = std::fread(dst, 1, len, f);
    std::fclose(f);
    return readBytes;
#endif
}

bool SdCacheStorage::beginWrite(const char* name) {
    endWrite(); // Close any existing write

    strncpy(_writeName, name, sizeof(_writeName) - 1);
    _writeName[sizeof(_writeName) - 1] = '\0';
    
    snprintf(_writeFinalPath, sizeof(_writeFinalPath), "%s/%s", _baseDir, name);
    snprintf(_writeTmpPath, sizeof(_writeTmpPath), "%s/%s.tmp", _baseDir, name);

#ifdef PLATFORM_ESP32
    _writeFile = SD_FS.open(_writeTmpPath, FILE_WRITE);
    return _writeFile;
#else
    _writeFile = std::fopen(_writeTmpPath, "w+b");
    return _writeFile != nullptr;
#endif
}

bool SdCacheStorage::write(const void* data, uint32_t len) {
#ifdef PLATFORM_ESP32
    if (!_writeFile) return false;
    return _writeFile.write(static_cast<const uint8_t*>(data), len) == len;
#else
    if (!_writeFile) return false;
    return std::fwrite(data, 1, len, _writeFile) == len;
#endif
}

bool SdCacheStorage::endWrite() {
#ifdef PLATFORM_ESP32
    if (!_writeFile) return false;
    _writeFile.close();
    if (SD_FS.exists(_writeFinalPath)) {
        SD_FS.remove(_writeFinalPath);
    }
    return SD_FS.rename(_writeTmpPath, _writeFinalPath);
#else
    if (!_writeFile) return false;
    std::fclose(_writeFile);
    _writeFile = nullptr;
    std::remove(_writeFinalPath);
    return std::rename(_writeTmpPath, _writeFinalPath) == 0;
#endif
}

int32_t SdCacheStorage::readBackAt(uint32_t offset, void* dst, uint32_t len) {
#ifdef PLATFORM_ESP32
    if (!_writeFile) return -1;
    // Save current pos
    uint32_t curPos = _writeFile.position();
    if (!_writeFile.seek(offset)) return -1;
    int32_t readBytes = _writeFile.read(static_cast<uint8_t*>(dst), len);
    _writeFile.seek(curPos);
    return readBytes;
#else
    if (!_writeFile) return -1;
    long curPos = std::ftell(_writeFile);
    if (std::fseek(_writeFile, offset, SEEK_SET) != 0) return -1;
    int32_t readBytes = std::fread(dst, 1, len, _writeFile);
    std::fseek(_writeFile, curPos, SEEK_SET);
    return readBytes;
#endif
}

} // namespace book
} // namespace eenk
