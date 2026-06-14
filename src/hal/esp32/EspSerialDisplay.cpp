#ifdef PLATFORM_ESP32

#include "EspSerialDisplay.h"
#include <Arduino.h>

EspSerialDisplay::EspSerialDisplay()
{
    // VT100 clear screen + move cursor to top-left
    Serial.print("\x1B[2J\x1B[H");
}

void EspSerialDisplay::clear()
{
    Serial.print("\x1B[2J\x1B[H");
}

void EspSerialDisplay::present()
{
    // Nothing to flush — each drawNarrativeLine/drawChoiceLine writes immediately.
}

void EspSerialDisplay::drawNarrativeLine(const char* text)
{
    Serial.println(text);
}

void EspSerialDisplay::drawChoiceLine(int index, const char* text, bool selected)
{
    if (selected) {
        // Bold + invert via VT100
        Serial.printf("\x1B[7m > %d. %s\x1B[0m\r\n", index + 1, text);
    } else {
        Serial.printf("   %d. %s\r\n", index + 1, text);
    }
}

void EspSerialDisplay::drawSeparator()
{
    Serial.println("────────────────────────────────────────");
}

#endif // PLATFORM_ESP32
