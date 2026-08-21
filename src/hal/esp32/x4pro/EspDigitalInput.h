#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IInput.h"
#include "Gt911Driver.h"

class EspDigitalInput : public IInput
{
public:
    EspDigitalInput();
    ~EspDigitalInput() override = default;

    ButtonEvent pollInput() override;
    bool getTouchPosition(int& x, int& y) const override;
    uint32_t getLastActivityTime() const override { return _lastActivityMs; }
    void setAutoSleepTimeout(uint16_t seconds) override { _timeoutSec = seconds; }
    void resetActivityTimer() override;

    // Touch access for Phase 2/3 UI
    Gt911Driver::TouchState getTouchState() const { return _lastTouch; }

private:
    uint32_t _lastActivityMs;
    uint16_t _timeoutSec;
    Gt911Driver _touch;
    Gt911Driver::TouchState _lastTouch;

// Uncomment to enable touch debug output to Serial
#define TOUCH_DEBUG

    bool _isSwiping = false;
    int _swipeStartX = -1;
    int _swipeStartY = -1;
    int _swipeLastX = -1;
    int _swipeLastY = -1;
    uint32_t _swipeStartTime = 0;

    mutable bool _hasTap = false;
    mutable int _tapX = -1;
    mutable int _tapY = -1;

    static constexpr int PIN_LEFT  = 0;   // Left button -> LEFT
    static constexpr int PIN_RIGHT = 7;   // Right button -> RIGHT
    static constexpr int PIN_POWER = 3;   // Power button -> CONFIRM / SLEEP
};

#endif
