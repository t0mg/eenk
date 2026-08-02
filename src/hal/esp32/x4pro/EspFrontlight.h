#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IFrontlight.h"

class EspFrontlight : public IFrontlight {
public:
    EspFrontlight();
    ~EspFrontlight() override = default;

    void setBrightness(uint8_t percent) override;
    void setWarmth(uint8_t percent) override;
    uint8_t getBrightness() const override { return _brightness; }
    uint8_t getWarmth() const override { return _warmth; }

private:
    uint8_t _brightness = 0;
    uint8_t _warmth = 0;

    static constexpr int PIN_COOL = 8;
    static constexpr int PIN_WARM = 9;
    static constexpr int CHANNEL_COOL = 0;
    static constexpr int CHANNEL_WARM = 1;
    static constexpr int PWM_FREQ = 5000;
    static constexpr int PWM_RESOLUTION = 8; // 0-255
};

#endif
