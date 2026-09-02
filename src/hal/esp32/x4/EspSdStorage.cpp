#include "EspSdStorage.h"
#include <Arduino.h>
#include <SPI.h>
#include <cstdlib>

EspSdStorage::EspSdStorage()
{
}

EspSdStorage::~EspSdStorage()
{
}

bool EspSdStorage::begin()
{
    if (_initialized) return true;

    // The SPI bus is already initialized by EspEinkDisplay.
    // SD.begin() will use the existing SPI instance — we only pass the CS pin.
    // Do NOT call SPI.begin() here; it would reconfigure the bus and crash
    // because GPIO12 is also SPIHD on ESP32-C3.
    if (!SD.begin(SD_CS_PIN, SPI, 40000000)) {
        Serial.println("[SD] SD card init failed");
        return false;
    }

    Serial.println("[SD] SD card detected");
    _initialized = true;
    return true;
}

const unsigned char* EspSdStorage::readFileBinary(const char* path, std::size_t* outSize)
{
    if (!_initialized) return nullptr;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[SD] File not found: %s\n", path);
        return nullptr;
    }

    std::size_t size = file.size();
    unsigned char* buf = (unsigned char*)std::malloc(size);
    if (!buf) {
        Serial.printf("[SD] malloc failed for %u bytes\n", (unsigned)size);
        file.close();
        return nullptr;
    }

    size_t bytesRead = file.read(buf, size);
    file.close();

    if (bytesRead != size) {
        Serial.printf("[SD] Short read: got %u of %u bytes\n", (unsigned)bytesRead, (unsigned)size);
        std::free(buf);
        return nullptr;
    }

    if (outSize) {
        *outSize = size;
    }
    Serial.printf("[SD] Read %u bytes from %s\n", (unsigned)size, path);
    return buf;
}

void EspSdStorage::freeBuffer(const unsigned char* buf)
{
    if (buf) {
        std::free((void*)buf);
    }
}

class EspSdFileWriter : public IFileWriter {
    File& _f;
public:
    explicit EspSdFileWriter(File& f) : _f(f) {}
    bool write(const void* data, std::size_t size) override {
        if (!_f) return false;
        return _f.write(static_cast<const uint8_t*>(data), size) == size;
    }
};

bool EspSdStorage::writeStream(const char* path, const std::function<bool(IFileWriter&)>& writer)
{
    if (!_initialized) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    EspSdFileWriter fw(file);
    bool ok = writer(fw);
    file.close();
    return ok;
}

class EspSdFileReader : public IFileReader {
    File& _f;
public:
    explicit EspSdFileReader(File& f) : _f(f) {}
    size_t read(void* dest, size_t size) override {
        if (!_f) return 0;
        return _f.read(static_cast<uint8_t*>(dest), size);
    }
    bool seek(size_t offset) override {
        if (!_f) return false;
        return _f.seek(offset);
    }
    size_t tell() const override {
        if (!_f) return 0;
        return _f.position();
    }
    size_t size() const override {
        if (!_f) return 0;
        return _f.size();
    }
};

bool EspSdStorage::readStream(const char* path, const std::function<bool(IFileReader&)>& reader)
{
    if (!_initialized) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    EspSdFileReader fr(file);
    bool ok = reader(fr);
    file.close();
    return ok;
}

bool EspSdStorage::renameFile(const char* oldPath, const char* newPath)
{
    if (!_initialized) return false;
    if (SD.exists(newPath)) {
        SD.remove(newPath);
    }
    return SD.rename(oldPath, newPath);
}

bool EspSdStorage::writeFileBinary(const char* path, const unsigned char* data, std::size_t size)
{
    if (!_initialized) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    size_t written = file.write(data, size);
    file.close();
    return written == size;
}

bool EspSdStorage::deleteFile(const char* path)
{
    if (!_initialized) return false;
    return SD.remove(path);
}

bool EspSdStorage::fileExists(const char* path)
{
    if (!_initialized) return false;
    return SD.exists(path);
}
