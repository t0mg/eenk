#ifdef PLATFORM_ESP32

#include "EspEinkDisplay.h"
#include <SPI.h>

// Display SPI pins (Xteink X4)
#define EPD_SCLK 8
#define EPD_MOSI 10
#define EPD_CS 21
#define EPD_DC 4
#define EPD_RST 5
#define EPD_BUSY 6

#include "BoardConfig.h"

EspEinkDisplay::EspEinkDisplay() 
    : _eink(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
    , _gfxRenderer(_eink)
{
    if (BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3) {
        _eink.setDisplayX3();
    }
    SPI.begin(EPD_SCLK, 7, EPD_MOSI, EPD_CS);
    _eink.begin();

    _gfxRenderer.begin();
    // Hardware has LandscapeCounterClockwise natively or we can just use Portrait to map it!
    // Papyrix's Portrait handles mapping to the 800x480 frame buffer.
    _gfxRenderer.setOrientation(GfxRenderer::Portrait);

    clear();
}

void EspEinkDisplay::clear()
{
    _gfxRenderer.clearScreen();
}

void EspEinkDisplay::present()
{
    _eink.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void EspEinkDisplay::fullRefresh()
{
    _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
}

#endif
