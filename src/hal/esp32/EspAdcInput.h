#pragma once
#ifdef PLATFORM_ESP32

#include "../IInput.h"
#include <InputManager.h>

class EspAdcInput : public IInput
{
public:
    EspAdcInput();
    ~EspAdcInput() override = default;

    ButtonEvent pollInput() override;

private:
    InputManager _input;
};

#endif
