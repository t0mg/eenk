#ifdef PLATFORM_ESP32

#include "EspSerialInput.h"
#include <Arduino.h>

EspSerialInput::EspSerialInput()
{
}

EspSerialInput::~EspSerialInput()
{
}

ButtonEvent EspSerialInput::pollInput()
{
    if (Serial.available() > 0) {
        int c = Serial.read();
        switch (c) {
        case 'w':
        case 'W':
        case 'A': // Arrow Up in VT100 sequence is \x1b[A, we might catch the 'A'
            return ButtonEvent::UP;
        case 's':
        case 'S':
        case 'B': // Arrow Down is \x1b[B
            return ButtonEvent::DOWN;
        case '\r':
        case '\n':
        case ' ': // Space also works to confirm
            return ButtonEvent::CONFIRM;
        case 'q':
        case 'Q':
            return ButtonEvent::QUIT;
        case 'b':
        case '\b': // Backspace
            return ButtonEvent::BACK;
        default:
            return ButtonEvent::NONE;
        }
    }
    return ButtonEvent::NONE;
}

#endif // PLATFORM_ESP32
