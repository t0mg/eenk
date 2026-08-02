#pragma once
#include <cstdint>

/**
 * eenk — IFrontlight: Platform-agnostic frontlight interface
 */
class IFrontlight {
public:
    virtual ~IFrontlight() = default;

    /** Set overall brightness level (0–100%). */
    virtual void setBrightness(uint8_t percentage) = 0;
    virtual uint8_t getBrightness() const = 0;

    /** Set color temperature split (0% = 100% cool, 100% = 100% warm). */
    virtual void setColorTemperature(uint8_t percentage) = 0;
    virtual uint8_t getColorTemperature() const = 0;
};
