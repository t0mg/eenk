#pragma once
#include <LittleFS.h>
#ifndef PLATFORM_NATIVE
using File = fs::File;
#endif

class FsFile {
private:
    File file;
public:
    FsFile() {}
    FsFile(File f) : file(f) {}
    
    int read(void* buf, size_t nbyte) { return file.read((uint8_t*)buf, nbyte); }
    int read() { return file.read(); }
    void seekSet(uint32_t pos) { file.seek(pos); }
    bool seek(uint32_t pos) { return file.seek(pos); }
    void close() { file.close(); }
    uint32_t position() const { return file.position(); }
    operator bool() const { return file; }
    bool seekCur(int32_t offset) { return file.seek(file.position() + offset); }
    uint32_t size() const { return file.size(); }
    
    File& getFile() { return file; }
};
