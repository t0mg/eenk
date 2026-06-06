#ifdef PLATFORM_ESP32

#include "EspSerialDisplay.h"
#include <Arduino.h>

EspSerialDisplay::EspSerialDisplay()
{
    // Make sure Serial is initialized in main.cpp before using this class.
}

EspSerialDisplay::~EspSerialDisplay()
{
}

void EspSerialDisplay::clear()
{
    // Clear screen and home cursor
    Serial.print("\x1B[2J\x1B[H");
}

void EspSerialDisplay::drawText(int col, int row, const char* text, bool inverted)
{
    // VT100 cursor positioning is 1-indexed (row;col)
    // We add 1 because our col/row are 0-indexed.
    Serial.printf("\x1B[%d;%dH", row + 1, col + 1);

    if (inverted) {
        Serial.print("\x1B[7m"); // Reverse video
    }

    Serial.print(text);

    if (inverted) {
        Serial.print("\x1B[0m"); // Reset attributes
    }
}

void EspSerialDisplay::drawHLine(int y)
{
    // Move to the row and draw dashes
    Serial.printf("\x1B[%d;1H", y + 1);
    for (int i = 0; i < getCols(); ++i) {
        Serial.print("-");
    }
}

void EspSerialDisplay::present()
{
    // For a terminal, output is already flushed via Serial.print.
    // We just move the cursor out of the way to the bottom.
    Serial.printf("\x1B[%d;1H", getRows());
}

void EspSerialDisplay::fullRefresh()
{
    clear();
    present();
}

#endif // PLATFORM_ESP32
