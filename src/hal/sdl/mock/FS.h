#pragma once
#include <cstddef>
#include <cstdint>
class File {
public:
    operator bool() const { return false; }
    void close() {}
    int read(void* buf, size_t nbyte) { return 0; }
    int read() { return 0; }
    bool seek(uint32_t pos) { return true; }
    void seekSet(uint32_t pos) {}
    uint32_t position() const { return 0; }
    uint32_t size() const { return 0; }
};

class FS {
public:
    bool begin() { return false; }
    File open(const char* path, const char* mode = "r") { return File(); }
    File open(const char* path, int flags) { return File(); }
};
extern FS LittleFS;
