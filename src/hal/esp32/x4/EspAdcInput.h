#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IInput.h"
#include <InputManager.h>

class EspAdcInput : public IInput
{
public:
    EspAdcInput();
    ~EspAdcInput() override = default;

    ButtonEvent pollInput() override;
    uint32_t getLastActivityTime() const override { return _lastActivityMs; }
    void setAutoSleepTimeout(uint16_t seconds) override { _timeoutSec = seconds; }
    void resetActivityTimer() override;

private:
    InputManager _input;
    uint32_t _lastActivityMs;
    uint16_t _timeoutSec;
};

#endif
