#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IFrontlight.h"
#include <FrontlightManager.h>

class EspFrontlight : public IFrontlight {
public:
  EspFrontlight() : _frontlightManager() { _frontlightManager.begin(); };
  ~EspFrontlight() override = default;

  void on() override { _frontlightManager.on(); }
  void off() override { _frontlightManager.off(); }
  void setBrightness(uint8_t percent) override {
    _frontlightManager.setBrightness(percent);
  }
  void setColorTemperature(uint8_t percent) override {
    _frontlightManager.setColorTemperature(percent);
  }
  uint8_t getBrightness() const override {
    return _frontlightManager.brightness();
  }
  uint8_t getColorTemperature() const override {
    return _frontlightManager.colorTemperature();
  }

private:
  FrontlightManager _frontlightManager;
};

#endif
