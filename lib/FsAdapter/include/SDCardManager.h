#pragma once

// SDCardManager — FS abstraction for EpdFontLoader / StreamingEpdFont.
//
// ESP32:  routes to the real SD card via SD.h.
//         Previously stubbed to LittleFS — this prevented StreamingEpdFont from
//         loading .epdfont files on-device.
//
// Native: routes to stdio fopen/fread so the simulator can load .epdfont files
//         from the local filesystem alongside the stories/ directory.
//
// Both platforms expose a common type alias `SdFile` for the file handle so
// library code (EpdFontLoader, StreamingEpdFont) can use it without conditionals.

#ifdef PLATFORM_ESP32
// ── ESP32 ────────────────────────────────────────────────────────────────────
#include <SD.h>

using SdFile = File;  // Arduino SD.h File

class SDCardManager {
public:
    static SDCardManager& getInstance() { static SDCardManager instance; return instance; }

    SdFile openFile(const char* path) { return SD.open(path, FILE_READ); }
    SdFile open(const char* path, int /*mode*/) { return SD.open(path, FILE_READ); }

    bool openFileForRead(const char* /*type*/, const char* path, SdFile& file) {
        file = SD.open(path, FILE_READ);
        return (bool)file;
    }

    bool openFileForWrite(const char* /*type*/, const char* path, SdFile& file) {
        file = SD.open(path, FILE_WRITE);
        return (bool)file;
    }
};

#else
// ── Native (simulator) ───────────────────────────────────────────────────────
#include <cstdio>
#include <cstring>
#include <cstdint>   // uint32_t

// Minimal file wrapper backed by stdio FILE*.
// Named NativeFile to avoid conflicting with the mock 'class File' in LittleFS.h.
class NativeFile {
public:
    NativeFile() : _fp(nullptr) {}
    explicit NativeFile(FILE* fp) : _fp(fp) {}
    ~NativeFile() {}  // caller responsible for close()

    explicit operator bool() const { return _fp != nullptr; }

    size_t read(void* buf, size_t len) {
        if (!_fp) return 0;
        return fread(buf, 1, len, _fp);
    }

    int read() {
        if (!_fp) return -1;
        return fgetc(_fp);
    }

    bool seek(size_t pos) {
        if (!_fp) return false;
        return fseek(_fp, (long)pos, SEEK_SET) == 0;
    }

    size_t position() {
        if (!_fp) return 0;
        return (size_t)ftell(_fp);
    }

    size_t size() {
        if (!_fp) return 0;
        long cur = ftell(_fp);
        fseek(_fp, 0, SEEK_END);
        long sz = ftell(_fp);
        fseek(_fp, cur, SEEK_SET);
        return (size_t)sz;
    }

    bool available() {
        if (!_fp) return false;
        return !feof(_fp);
    }

    void close() {
        if (_fp) { fclose(_fp); _fp = nullptr; }
    }

private:
    FILE* _fp;
};

using SdFile = NativeFile;  // platform-agnostic alias used by EpdFontLoader / StreamingEpdFont

class SDCardManager {
public:
    static SDCardManager& getInstance() { static SDCardManager instance; return instance; }

    SdFile openFile(const char* path) { return SdFile(fopen(path, "rb")); }
    SdFile open(const char* path, int /*mode*/) { return SdFile(fopen(path, "rb")); }

    bool openFileForRead(const char* /*type*/, const char* path, SdFile& file) {
        FILE* fp = fopen(path, "rb");
        file = SdFile(fp);
        return fp != nullptr;
    }

    bool openFileForWrite(const char* /*type*/, const char* path, SdFile& file) {
        FILE* fp = fopen(path, "wb");
        file = SdFile(fp);
        return fp != nullptr;
    }
};

#endif  // PLATFORM_ESP32

#define SdMan SDCardManager::getInstance()
