#pragma once

#include "hal/IDisplay.h"

/**
 * EENK — EspSerialDisplay
 *
 * Implements IDisplay using VT100/ANSI escape codes over the ESP32 Serial port.
 * This is used for Milestone 2 simulation in Wokwi before integrating the
 * real e-ink display driver.
 */
class EspSerialDisplay : public IDisplay
{
public:
    EspSerialDisplay();
    virtual ~EspSerialDisplay();

    void clear() override;
    void drawText(int col, int row, const char* text, bool inverted = false) override;
    void drawHLine(int y) override;
    void present() override;
    void fullRefresh() override;

    int getWidth() const override { return 80; }
    int getHeight() const override { return 24; }
    int getLineHeight() const override { return 1; }
    int getCharWidth() const override { return 1; }
    int getCols() const override { return 80; }
    int getRows() const override { return 24; }
};
