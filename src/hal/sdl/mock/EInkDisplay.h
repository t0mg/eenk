#pragma once
#include <cstdint>
#include <cstddef>

class EInkDisplay {
public:
    static constexpr uint16_t DISPLAY_WIDTH = 800;
    static constexpr uint16_t DISPLAY_HEIGHT = 480;
    static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
    static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;
    static constexpr uint16_t X3_DISPLAY_WIDTH = 792;
    static constexpr uint16_t X3_DISPLAY_HEIGHT = 528;
    static constexpr uint16_t X3_DISPLAY_WIDTH_BYTES = X3_DISPLAY_WIDTH / 8;
    static constexpr uint32_t X3_BUFFER_SIZE = X3_DISPLAY_WIDTH_BYTES * X3_DISPLAY_HEIGHT;
    static constexpr uint32_t MAX_BUFFER_SIZE = 65536;

    enum RefreshMode {
        FULL_REFRESH,
        HALF_REFRESH,
        FAST_REFRESH
    };

    explicit EInkDisplay(uint16_t w = DISPLAY_WIDTH, uint16_t h = DISPLAY_HEIGHT)
        : _width(w), _height(h), _widthBytes((w + 7) / 8), _bufferSize(_widthBytes * _height) {}

    void setDimensions(uint16_t w, uint16_t h) {
        _width = w;
        _height = h;
        _widthBytes = (w + 7) / 8;
        _bufferSize = _widthBytes * _height;
    }

    uint16_t getDisplayWidth() const { return _width; }
    uint16_t getDisplayHeight() const { return _height; }
    uint16_t getDisplayWidthBytes() const { return _widthBytes; }
    uint32_t getBufferSize() const { return _bufferSize; }

    uint8_t* getFrameBuffer() const { return _frameBuffer; }

    void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false) {}
    void displayBufferDriveAll(bool turnOffScreen = false) {}
    void cleanupGrayscaleBuffers(uint8_t* fb) const {}
    
    void drawImage(const uint8_t* bitmap, int x, int y, int w, int h) const {
        const int imageWidthBytes = (w + 7) / 8;
        for (int row = 0; row < h; row++) {
            int destY = y + row;
            if (destY >= _height) break;
            int destOffset = destY * _widthBytes + (x / 8);
            int srcOffset = row * imageWidthBytes;
            for (int col = 0; col < imageWidthBytes; col++) {
                if ((x / 8 + col) >= _widthBytes) break;
                _frameBuffer[destOffset + col] = bitmap[srcOffset + col];
            }
        }
    }
    void displayWindow(int x, int y, int w, int h, bool turnOffScreen = false) const {}
    void grayscaleRevert() const {}
    void copyGrayscaleLsbBuffers(uint8_t* fb) const {}
    void copyGrayscaleMsbBuffers(uint8_t* fb) const {}
    void displayGrayBuffer(bool turnOffScreen = false) const {}

    void clearScreen(uint8_t color = 0xFF) const {
        for (uint32_t i = 0; i < _bufferSize; ++i) _frameBuffer[i] = color;
    }

private:
    uint16_t _width = DISPLAY_WIDTH;
    uint16_t _height = DISPLAY_HEIGHT;
    uint16_t _widthBytes = DISPLAY_WIDTH_BYTES;
    uint32_t _bufferSize = BUFFER_SIZE;
    mutable uint8_t _frameBuffer[MAX_BUFFER_SIZE] = {0};
};
