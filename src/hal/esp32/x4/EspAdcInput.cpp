#ifdef PLATFORM_ESP32

#include "EspAdcInput.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

EspAdcInput::EspAdcInput() : _lastActivityMs(millis()), _timeoutSec(0) {
  _input.begin();
}

void EspAdcInput::resetActivityTimer() {
  _lastActivityMs = millis();
}

ButtonEvent EspAdcInput::pollInput() {
  _input.update();

  ButtonEvent ev = ButtonEvent::NONE;

  if (_input.isPressed(InputManager::BTN_POWER) &&
      _input.getHeldTime() > 1000) {
    // Wait for the user to release the button so we don't immediately wake up!
    while (digitalRead(InputManager::POWER_BUTTON_PIN) == LOW) {
      delay(10);
    }
    ev = ButtonEvent::SLEEP;
  } else if (_input.wasPressed(InputManager::BTN_UP))
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
  else if (_input.wasReleased(InputManager::BTN_POWER))
    ev = ButtonEvent::CONFIRM;

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
