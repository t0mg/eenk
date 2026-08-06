#pragma once
#include <stdint.h>

/**
 * Interface for e-ink frontlight PWM control (Cool & Warm LEDs).
 */
class IFrontlight {
public:
  virtual ~IFrontlight() = default;

  virtual void on() = 0;
  virtual void off() = 0;
  virtual void setBrightness(uint8_t percent) = 0;       // 0-100% cool LED
  virtual void setColorTemperature(uint8_t percent) = 0; // 0-100% warm LED
  virtual uint8_t getBrightness() const = 0;
  virtual uint8_t getColorTemperature() const = 0;
};
