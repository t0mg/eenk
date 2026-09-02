#pragma once
#include <cstddef>
#include <functional>

/**
 * Interface for streaming writes to avoid buffering large files in heap.
 */
class IFileWriter
{
public:
    virtual ~IFileWriter() = default;
    virtual bool write(const void* data, std::size_t size) = 0;
};

/**
 * Interface for streaming reads to avoid loading large files into heap.
 */
class IFileReader
{
public:
    virtual ~IFileReader() = default;
    virtual std::size_t read(void* dest, std::size_t size) = 0;
    virtual bool seek(std::size_t offset) = 0;
    virtual std::size_t tell() const = 0;
    virtual std::size_t size() const = 0;
};

/**
 * eenk — IStorage: Platform-agnostic storage interface
 *
 * Implemented by:
 *   SDLStorage      (PLATFORM_NATIVE)  — std::fopen / std::fread
 *   SDCardStorage   (PLATFORM_ESP32)   — SD card via SPI (M2)
 */

class IStorage
{
public:
    virtual ~IStorage() = default;

    /**
     * Read an entire file into a freshly allocated buffer.
     * @param path    File path (platform-specific separator)
     * @param outSize Written with the number of bytes read on success
     * @return        Heap-allocated buffer, or nullptr on failure.
     *                Caller must call freeBuffer() when done.
     */
    virtual const unsigned char* readFileBinary(const char* path, std::size_t* outSize) = 0;

    /**
     * Release a buffer previously returned by readFileBinary().
     * Safe to call with nullptr.
     */
    virtual void freeBuffer(const unsigned char* buf) = 0;

    /**
     * Write data to a binary file.
     * @param path File path
     * @param data Pointer to buffer
     * @param size Size in bytes
     * @return True on success
     */
    virtual bool writeFileBinary(const char* path, const unsigned char* data, std::size_t size) {
        return writeStream(path, [data, size](IFileWriter& writer) {
            return writer.write(data, size);
        });
    }

    /**
     * Stream binary data to a file via an IFileWriter callback.
     * Avoids holding large serialized buffers in RAM.
     * @param path File path
     * @param writer Callback invoked with an IFileWriter reference
     * @return True if file was opened, written, and closed successfully
     */
    virtual bool writeStream(const char* path, const std::function<bool(IFileWriter&)>& writer) = 0;

    /**
     * Stream binary data from a file via an IFileReader callback.
     * Avoids holding large file buffers in RAM.
     * @param path File path
     * @param reader Callback invoked with an IFileReader reference
     * @return True if file was opened, read, and closed successfully
     */
    virtual bool readStream(const char* path, const std::function<bool(IFileReader&)>& reader) = 0;

    /**
     * Rename a file.
     * @param oldPath Source file path
     * @param newPath Destination file path
     * @return True on success
     */
    virtual bool renameFile(const char* oldPath, const char* newPath) = 0;

    /**
     * Delete a file.
     * @param path File path
     * @return True on success
     */
    virtual bool deleteFile(const char* path) = 0;

    /** Returns true if the file exists and is readable. */
    virtual bool fileExists(const char* path) = 0;
};
