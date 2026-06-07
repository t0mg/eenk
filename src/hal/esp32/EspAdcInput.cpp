#ifdef PLATFORM_ESP32

#include "EspAdcInput.h"

EspAdcInput::EspAdcInput()
{
    _input.begin();
}

ButtonEvent EspAdcInput::pollInput()
{
    _input.update();

    if (_input.wasPressed(InputManager::BTN_UP))      return ButtonEvent::UP;
    if (_input.wasPressed(InputManager::BTN_DOWN))    return ButtonEvent::DOWN;
    if (_input.wasPressed(InputManager::BTN_LEFT))    return ButtonEvent::LEFT;
    if (_input.wasPressed(InputManager::BTN_RIGHT))   return ButtonEvent::RIGHT;
    if (_input.wasPressed(InputManager::BTN_CONFIRM)) return ButtonEvent::CONFIRM;
    if (_input.wasPressed(InputManager::BTN_BACK))    return ButtonEvent::BACK;
    if (_input.wasPressed(InputManager::BTN_POWER))   return ButtonEvent::QUIT;

    return ButtonEvent::NONE;
}

#endif
