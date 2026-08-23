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
