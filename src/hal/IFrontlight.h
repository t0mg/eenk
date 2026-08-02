#pragma once
#include <stdint.h>

/**
 * Interface for e-ink frontlight PWM control (Cool & Warm LEDs).
 */
class IFrontlight {
public:
    virtual ~IFrontlight() = default;

    virtual void setBrightness(uint8_t percent) = 0; // 0-100% cool LED
    virtual void setWarmth(uint8_t percent) = 0;     // 0-100% warm LED
    virtual uint8_t getBrightness() const = 0;
    virtual uint8_t getWarmth() const = 0;
};
