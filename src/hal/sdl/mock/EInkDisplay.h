#pragma once
#include <cstdint>
#include <cstddef>

class EInkDisplay {
public:
    static constexpr uint16_t DISPLAY_WIDTH = 800;
    static constexpr uint16_t DISPLAY_HEIGHT = 480;
    static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
    static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;
    static constexpr uint32_t MAX_BUFFER_SIZE = BUFFER_SIZE;

    enum RefreshMode {
        FULL_REFRESH,
        HALF_REFRESH,
        FAST_REFRESH
    };

    uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
    uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
    uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
    uint32_t getBufferSize() const { return BUFFER_SIZE; }

    uint8_t* getFrameBuffer() const { return _frameBuffer; }

    void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false) {}
    void displayBufferDriveAll(bool turnOffScreen = false) {}
    void cleanupGrayscaleBuffers(uint8_t* fb) const {}
    
    void drawImage(const uint8_t* bitmap, int x, int y, int w, int h) const {}
    void displayWindow(int x, int y, int w, int h, bool turnOffScreen = false) const {}
    void grayscaleRevert() const {}
    void copyGrayscaleLsbBuffers(uint8_t* fb) const {}
    void copyGrayscaleMsbBuffers(uint8_t* fb) const {}
    void displayGrayBuffer(bool turnOffScreen = false) const {}

    void clearScreen(uint8_t color = 0xFF) const {
        for (uint32_t i = 0; i < BUFFER_SIZE; ++i) _frameBuffer[i] = color;
    }

private:
    mutable uint8_t _frameBuffer[BUFFER_SIZE] = {0};
};
