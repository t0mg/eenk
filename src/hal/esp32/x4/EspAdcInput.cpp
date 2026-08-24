#ifdef PLATFORM_ESP32

#include "EspAdcInput.h"
#include <BoardConfig.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

EspAdcInput::EspAdcInput() : _lastActivityMs(millis()), _timeoutSec(0) {
  _input.begin();
}

void EspAdcInput::resetActivityTimer() {
  _lastActivityMs = millis();
}

ButtonEvent EspAdcInput::pollInput() {
  ButtonEvent ev = ButtonEvent::NONE;

  const int powerPin = BoardConfig::ACTIVE.input.power;
  const int powerActiveLevel = BoardConfig::ACTIVE.input.powerActiveHigh ? HIGH : LOW;

  if (powerPin >= 0 && digitalRead(powerPin) == powerActiveLevel) {
    unsigned long start = millis();
    while (digitalRead(powerPin) == powerActiveLevel) {
      if (millis() - start > 1000) {
        while (digitalRead(powerPin) == powerActiveLevel) {
          delay(10);
        }
        ev = ButtonEvent::SLEEP;
        break;
      }
      delay(10);
    }
    if (ev != ButtonEvent::SLEEP) {
      ev = ButtonEvent::CONFIRM;
    }
  } else {
    _input.update();

    if (_input.wasPressed(InputManager::BTN_UP))
      ev = ButtonEvent::UP;
    else if (_input.wasPressed(InputManager::BTN_DOWN))
      ev = ButtonEvent::DOWN;
    else if (_input.wasPressed(InputManager::BTN_LEFT))
      ev = ButtonEvent::LEFT;
    else if (_input.wasPressed(InputManager::BTN_RIGHT))
      ev = ButtonEvent::RIGHT;
    else if (_input.wasPressed(InputManager::BTN_CONFIRM))
      ev = ButtonEvent::CONFIRM;
    else if (_input.wasPressed(InputManager::BTN_BACK))
      ev = ButtonEvent::QUIT;
  }

  if (ev != ButtonEvent::NONE) {
    _lastActivityMs = millis();
  } else if (_timeoutSec > 0 && _lastActivityMs > 0 &&
             (millis() - _lastActivityMs > _timeoutSec * 1000UL)) {
    _lastActivityMs = millis();
    return ButtonEvent::SLEEP;
  }
  return ev;
}

#endif

