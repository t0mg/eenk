#pragma once
#ifdef PLATFORM_NATIVE

#include "hal/IStorage.h"
#include <cstddef>

/**
 * eenk — SDLStorage
 *
 * Native desktop implementation of IStorage using std::fopen / std::fread.
 * readFileBinary() allocates with new[] — always call freeBuffer() when done.
 */
class SDLStorage : public IStorage
{
public:
    SDLStorage()          = default;
    ~SDLStorage() override = default;

    const unsigned char* readFileBinary(const char* path,
                                        std::size_t* outSize) override;
    void freeBuffer(const unsigned char* buf) override;
    bool writeFileBinary(const char* path, const unsigned char* data, std::size_t size) override;
    bool deleteFile(const char* path) override;
    bool fileExists(const char* path)         override;
};

#endif // PLATFORM_NATIVE
