#ifdef PLATFORM_NATIVE

#include "SDLStorage.h"
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
const unsigned char* SDLStorage::readFileBinary(const char* path, std::size_t* outSize)
{
    if (outSize) *outSize = 0;

    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[SDLStorage] Cannot open: %s\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return nullptr;
    }

    unsigned char* buf = new unsigned char[static_cast<std::size_t>(size)];
    std::size_t read = fread(buf, 1, static_cast<std::size_t>(size), f);
    fclose(f);

    if (read != static_cast<std::size_t>(size)) {
        fprintf(stderr, "[SDLStorage] Short read on %s\n", path);
        delete[] buf;
        return nullptr;
    }

    if (outSize) *outSize = read;
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLStorage::freeBuffer(const unsigned char* buf)
{
    delete[] buf;
}

class SDLFileWriter : public IFileWriter {
    FILE* _f;
public:
    explicit SDLFileWriter(FILE* f) : _f(f) {}
    bool write(const void* data, std::size_t size) override {
        if (!_f) return false;
        return std::fwrite(data, 1, size, _f) == size;
    }
};

bool SDLStorage::writeStream(const char* path, const std::function<bool(IFileWriter&)>& writer)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    SDLFileWriter fw(f);
    bool ok = writer(fw);
    fclose(f);
    return ok;
}

class SDLFileReader : public IFileReader {
    FILE* _f;
    size_t _size;
public:
    explicit SDLFileReader(FILE* f) : _f(f), _size(0) {
        if (_f) {
            long cur = std::ftell(_f);
            std::fseek(_f, 0, SEEK_END);
            long sz = std::ftell(_f);
            std::fseek(_f, cur, SEEK_SET);
            _size = sz >= 0 ? static_cast<size_t>(sz) : 0;
        }
    }
    size_t read(void* dest, size_t size) override {
        if (!_f) return 0;
        return std::fread(dest, 1, size, _f);
    }
    bool seek(size_t offset) override {
        if (!_f) return false;
        return std::fseek(_f, static_cast<long>(offset), SEEK_SET) == 0;
    }
    size_t tell() const override {
        if (!_f) return 0;
        long pos = std::ftell(_f);
        return pos >= 0 ? static_cast<size_t>(pos) : 0;
    }
    size_t size() const override {
        return _size;
    }
};

bool SDLStorage::readStream(const char* path, const std::function<bool(IFileReader&)>& reader)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    SDLFileReader fr(f);
    bool ok = reader(fr);
    fclose(f);
    return ok;
}

bool SDLStorage::renameFile(const char* oldPath, const char* newPath)
{
    std::remove(newPath);
    return std::rename(oldPath, newPath) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
bool SDLStorage::writeFileBinary(const char* path, const unsigned char* data, std::size_t size)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

// ─────────────────────────────────────────────────────────────────────────────
bool SDLStorage::deleteFile(const char* path)
{
    return remove(path) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
bool SDLStorage::fileExists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

#endif // PLATFORM_NATIVE
