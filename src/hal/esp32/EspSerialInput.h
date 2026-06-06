#pragma once

#include "hal/IInput.h"

/**
 * EENK — EspSerialInput
 *
 * Implements IInput by reading from the ESP32 Serial port.
 * W/S for Up/Down, Enter to Confirm, Esc/Q to Quit.
 */
class EspSerialInput : public IInput
{
public:
    EspSerialInput();
    virtual ~EspSerialInput();

    ButtonEvent pollInput() override;
};
