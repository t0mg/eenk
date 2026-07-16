#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

// Mock flags
#define FILE_READ 0
#define FILE_WRITE 1

class File {
    std::shared_ptr<FILE> _f;
    uint32_t _size = 0;
public:
    File() {}
    File(FILE* f) {
        if (f) {
            _f = std::shared_ptr<FILE>(f, [](FILE* ptr) { if (ptr) std::fclose(ptr); });
            long curr = std::ftell(f);
            std::fseek(f, 0, SEEK_END);
            _size = std::ftell(f);
            std::fseek(f, curr, SEEK_SET);
        }
    }
    operator bool() const { return _f != nullptr; }
    void close() { _f.reset(); }
    int read(void* buf, size_t nbyte) {
        if (!_f) return 0;
        return std::fread(buf, 1, nbyte, _f.get());
    }
    int read() {
        if (!_f) return -1;
        uint8_t b;
        if (std::fread(&b, 1, 1, _f.get()) == 1) return b;
        return -1;
    }
    bool seek(uint32_t pos) {
        if (!_f) return false;
        return std::fseek(_f.get(), pos, SEEK_SET) == 0;
    }
    void seekSet(uint32_t pos) { seek(pos); }
    uint32_t position() const {
        if (!_f) return 0;
        return std::ftell(_f.get());
    }
    uint32_t size() const { return _size; }
    bool isDirectory() const { return false; }
    std::string name() const { return ""; }
    File openNextFile() { return File(); }
};

class FS {
public:
    bool begin() { return true; }
    File open(const char* path, const char* mode = "r") {
        std::string m = mode;
        if (m == "r") m = "rb";
        if (m == "w") m = "wb";
        if (m == "a") m = "ab";
        FILE* f = std::fopen(path, m.c_str());
        if (f) return File(f);
        return File();
    }
    File open(const char* path, int flags) {
        return open(path, flags == FILE_WRITE ? "w" : "r");
    }
    bool exists(const char* path) {
        FILE* f = std::fopen(path, "rb");
        if (f) {
            std::fclose(f);
            return true;
        }
        return false;
    }
};
