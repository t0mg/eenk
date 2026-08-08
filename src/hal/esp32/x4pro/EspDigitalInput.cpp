#ifdef PLATFORM_ESP32

#include "EspDigitalInput.h"
#include <Arduino.h>

EspDigitalInput::EspDigitalInput()
    : _lastActivityMs(millis()), _timeoutSec(0), _touch(39, 38, 10, 4) {
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_POWER, INPUT_PULLUP);

  _touch.begin();
}

bool EspDigitalInput::getTouchPosition(int &x, int &y) const {
  if (_hasTap) {
    x = _tapX;
    y = _tapY;
    _hasTap = false;
    return true;
  }
  x = -1;
  y = -1;
  return false;
}

ButtonEvent EspDigitalInput::pollInput() {
  ButtonEvent ev = ButtonEvent::NONE;

  // Check physical GPIO buttons
  if (digitalRead(PIN_POWER) == LOW) {
    unsigned long start = millis();
    while (digitalRead(PIN_POWER) == LOW) {
      if (millis() - start > 1000) {
        while (digitalRead(PIN_POWER) == LOW) {
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
  } else if (digitalRead(PIN_LEFT) == LOW) {
    ev = ButtonEvent::LEFT;
    delay(150); // debounce
  } else if (digitalRead(PIN_RIGHT) == LOW) {
    ev = ButtonEvent::RIGHT;
    delay(150); // debounce
  }

  // Check capacitive Home key & touch via GT911
  if (ev == ButtonEvent::NONE) {
    _lastTouch = _touch.readState();
    if (_lastTouch.homePressed) {
#ifdef TOUCH_DEBUG
      Serial.println("[Touch] HOME pressed");
#endif
      ev = ButtonEvent::QUIT;
      delay(200);
    } else if (_lastTouch.touched) {
      if (!_isSwiping) {
        _isSwiping = true;
        _swipeStartX = _lastTouch.x;
        _swipeStartY = _lastTouch.y;
        _swipeLastX = _lastTouch.x;
        _swipeLastY = _lastTouch.y;
        _swipeStartTime = millis();
#ifdef TOUCH_DEBUG
        Serial.printf("[Touch] DOWN at (%d, %d)\n", _swipeStartX, _swipeStartY);
#endif
      } else {
        _swipeLastX = _lastTouch.x;
        _swipeLastY = _lastTouch.y;
      }
    } else if (_isSwiping) {
      _isSwiping = false;
      int dx = _swipeLastX - _swipeStartX;
      int dy = _swipeLastY - _swipeStartY;
      uint32_t dt = millis() - _swipeStartTime;
#ifdef TOUCH_DEBUG
      Serial.printf("[Touch] UP at (%d, %d) | dx=%d dy=%d dt=%lums\n",
                    _swipeLastX, _swipeLastY, dx, dy, (unsigned long)dt);
#endif
      if (dt < 800) {
        if (abs(dy) > abs(dx) && abs(dy) > 50) {
          if (dy > 0) {
            if (_swipeStartY < 60) {
              ev = ButtonEvent::TOP_EDGE_SWIPE;
#ifdef TOUCH_DEBUG
              Serial.println("[Touch] -> TOP_EDGE_SWIPE");
#endif
            } else {
              ev = ButtonEvent::SWIPE_DOWN;
#ifdef TOUCH_DEBUG
              Serial.println("[Touch] -> SWIPE_DOWN");
#endif
            }
          } else {
            ev = ButtonEvent::SWIPE_UP;
#ifdef TOUCH_DEBUG
            Serial.println("[Touch] -> SWIPE_UP");
#endif
          }
        } else if (abs(dx) > abs(dy) && abs(dx) > 50) {
          ev = (dx > 0) ? ButtonEvent::SWIPE_RIGHT : ButtonEvent::SWIPE_LEFT;
#ifdef TOUCH_DEBUG
          Serial.printf("[Touch] -> %s\n",
                        dx > 0 ? "SWIPE_RIGHT" : "SWIPE_LEFT");
#endif
        } else if (abs(dx) < 30 && abs(dy) < 30 && dt < 400) {
          _tapX = _swipeStartX;
          _tapY = _swipeStartY;
          _hasTap = true;
#ifdef TOUCH_DEBUG
          Serial.printf("[Touch] -> TAP at (%d, %d)\n", _tapX, _tapY);
#endif
        } else {
#ifdef TOUCH_DEBUG
          Serial.printf("[Touch] -> IGNORED (dt=%lu, dx=%d, dy=%d)\n",
                        (unsigned long)dt, dx, dy);
#endif
        }
      } else {
#ifdef TOUCH_DEBUG
        Serial.printf("[Touch] -> TIMEOUT dt=%lu (>800ms), ignored\n",
                      (unsigned long)dt);
#endif
      }
    }
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
