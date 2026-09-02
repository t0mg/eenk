#pragma once
#include "hal/IStorage.h"
#include <SD.h>

// SD card CS pin on Xteink X4 (shared SPI bus with e-ink display)
// SPI bus (SCLK=8, MISO=7, MOSI=10) is initialized by EspEinkDisplay.
// We only need to specify the chip-select pin here.
#define SD_CS_PIN 12

class EspSdStorage : public IStorage
{
public:
    EspSdStorage();
    ~EspSdStorage() override;

    // Initialize the SD card on the already-running SPI bus.
    // Must be called AFTER the display has initialized SPI.
    bool begin();

    const unsigned char* readFileBinary(const char* path, std::size_t* outSize) override;
    void freeBuffer(const unsigned char* buf) override;
    bool writeFileBinary(const char* path, const unsigned char* data, std::size_t size) override;
    bool writeStream(const char* path, const std::function<bool(IFileWriter&)>& writer) override;
    bool deleteFile(const char* path) override;
    bool fileExists(const char* path) override;

private:
    bool _initialized = false;
};
