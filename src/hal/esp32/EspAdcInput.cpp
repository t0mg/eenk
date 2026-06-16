#ifdef PLATFORM_ESP32

#include "EspAdcInput.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

EspAdcInput::EspAdcInput()
{
    _input.begin();
}

ButtonEvent EspAdcInput::pollInput()
{
    _input.update();

    if (_input.isPressed(InputManager::BTN_POWER) && _input.getHeldTime() > 2000) {
        // Wait for the user to release the button so we don't immediately wake up!
        while (digitalRead(InputManager::POWER_BUTTON_PIN) == LOW) {
            delay(10);
        }
        return ButtonEvent::SLEEP;
    }

    if (_input.wasPressed(InputManager::BTN_UP))      return ButtonEvent::UP;
    if (_input.wasPressed(InputManager::BTN_DOWN))    return ButtonEvent::DOWN;
    if (_input.wasPressed(InputManager::BTN_LEFT))    return ButtonEvent::LEFT;
    if (_input.wasPressed(InputManager::BTN_RIGHT))   return ButtonEvent::RIGHT;
    if (_input.wasPressed(InputManager::BTN_CONFIRM)) return ButtonEvent::CONFIRM;
    if (_input.wasPressed(InputManager::BTN_BACK))    return ButtonEvent::QUIT;
    if (_input.wasReleased(InputManager::BTN_POWER))  return ButtonEvent::CONFIRM;

    return ButtonEvent::NONE;
}

#endif
