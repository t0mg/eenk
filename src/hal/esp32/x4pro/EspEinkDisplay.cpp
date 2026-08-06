#ifdef PLATFORM_ESP32
#include "EspEinkDisplay.h"
#include <Arduino.h>
#include <SPI.h>
#include "BoardConfig.h"

#include <XteinkDetect.h>

#define EPD_SCLK 12
#define EPD_MOSI 11
#define EPD_CS 13
#define EPD_DC 18
#define EPD_RST 14
#define EPD_BUSY 6

EspEinkDisplay::EspEinkDisplay() 
    : _eink(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
    , _gfxRenderer(_eink)
{
    BoardConfig::selectDevice(BoardConfig::Board::XteinkX4Pro);
    
    // Auto-detect controller via display-bus probe (SSD1677 vs UC8179)
    if (freeink::applyXteinkDisplayController()) {
        Serial.println("[Display] Display controller promoted via XteinkDetect.");
    } else {
        Serial.println("[Display] Display controller kept default profile choice.");
    }

    _eink.begin();
    _gfxRenderer.begin();
}

void EspEinkDisplay::clear() {
    _eink.clearScreen(0xFF);
}

void EspEinkDisplay::present() {
    _eink.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void EspEinkDisplay::fullRefresh() {
    _eink.displayBuffer(EInkDisplay::FULL_REFRESH);
}

#endif // PLATFORM_ESP32
