#ifdef PLATFORM_ESP32

#include "EspFrontlight.h"
#include <Arduino.h>

EspFrontlight::EspFrontlight() {
    ledcSetup(CHANNEL_COOL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_COOL, CHANNEL_COOL);

    ledcSetup(CHANNEL_WARM, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_WARM, CHANNEL_WARM);

    setBrightness(0);
    setWarmth(0);
}

void EspFrontlight::setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    _brightness = percent;
    uint32_t duty = (percent * 255) / 100;
    ledcWrite(CHANNEL_COOL, duty);
}

void EspFrontlight::setWarmth(uint8_t percent) {
    if (percent > 100) percent = 100;
    _warmth = percent;
    uint32_t duty = (percent * 255) / 100;
    ledcWrite(CHANNEL_WARM, duty);
}

#endif
