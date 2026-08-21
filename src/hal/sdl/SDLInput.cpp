#ifdef PLATFORM_NATIVE

#include "SDLInput.h"
#include "SDLDisplay.h" // for signalQuit()
#include "mock/Arduino.h"
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
SDLInput::SDLInput(SDLDisplay* display)
    : _display(display), _lastActivityMs(millis()), _timeoutSec(0)
{}

void SDLInput::resetActivityTimer() {
    _lastActivityMs = millis();
}

bool SDLInput::getTouchPosition(int &x, int &y) const {
    if (_hasTap) {
        x = _tapX;
        y = _tapY;
        _hasTap = false;
        const_cast<SDLInput*>(this)->_lastActivityMs = millis();
        return true;
    }
    x = -1;
    y = -1;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
ButtonEvent SDLInput::pollInput()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            if (_display) _display->signalQuit();
            return ButtonEvent::QUIT;

        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                _isMouseDown = true;
                _mouseDownX = ev.button.x;
                _mouseDownY = ev.button.y;
                _mouseDownTime = millis();
                _lastActivityMs = millis();
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_LEFT && _isMouseDown) {
                _isMouseDown = false;
                int upX = ev.button.x;
                int upY = ev.button.y;
                int dx = upX - _mouseDownX;
                int dy = upY - _mouseDownY;
                uint32_t dt = millis() - _mouseDownTime;
                _lastActivityMs = millis();

                if (dt < 800) {
                    if (abs(dy) > abs(dx) && abs(dy) > 50) {
                        if (dy > 0) {
                            if (_mouseDownY < 60) {
                                return ButtonEvent::TOP_EDGE_SWIPE;
                            } else {
                                return ButtonEvent::SWIPE_DOWN;
                            }
                        } else {
                            return ButtonEvent::SWIPE_UP;
                        }
                    } else if (abs(dx) > abs(dy) && abs(dx) > 50) {
                        return (dx > 0) ? ButtonEvent::SWIPE_RIGHT : ButtonEvent::SWIPE_LEFT;
                    } else if (abs(dx) < 30 && abs(dy) < 30) {
                        _tapX = upX;
                        _tapY = upY;
                        _hasTap = true;
                        return ButtonEvent::NONE;
                    }
                }
            }
            break;

        case SDL_MOUSEWHEEL:
            if (ev.wheel.y > 0) {
                _lastActivityMs = millis();
                return ButtonEvent::UP;
            } else if (ev.wheel.y < 0) {
                _lastActivityMs = millis();
                return ButtonEvent::DOWN;
            }
            break;

        case SDL_KEYDOWN: {
            SDL_Keycode key = ev.key.keysym.sym;
            switch (key) {
            // UP
            case SDLK_UP:    case SDLK_w: case SDLK_k:
                _lastActivityMs = millis();
                return ButtonEvent::UP;
            // DOWN
            case SDLK_DOWN:  case SDLK_s: case SDLK_j:
                _lastActivityMs = millis();
                return ButtonEvent::DOWN;
            // LEFT
            case SDLK_LEFT:  case SDLK_a: case SDLK_h:
                _lastActivityMs = millis();
                return ButtonEvent::LEFT;
            // RIGHT
            case SDLK_RIGHT: case SDLK_d: case SDLK_l:
                _lastActivityMs = millis();
                return ButtonEvent::RIGHT;
            // CONFIRM
            case SDLK_RETURN: case SDLK_RETURN2: case SDLK_SPACE:
                _lastActivityMs = millis();
                return ButtonEvent::CONFIRM;
            // BACK / QUIT
            case SDLK_ESCAPE: case SDLK_BACKSPACE:
                _lastActivityMs = millis();
                return ButtonEvent::QUIT;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }

    if (_hasTap) {
        return ButtonEvent::NONE;
    }

    if (_timeoutSec > 0 && _lastActivityMs > 0 && (millis() - _lastActivityMs > _timeoutSec * 1000UL)) {
        _lastActivityMs = millis();
        return ButtonEvent::SLEEP;
    }

    return ButtonEvent::NONE;
}

#endif // PLATFORM_NATIVE
