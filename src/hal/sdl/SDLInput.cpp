#ifdef PLATFORM_NATIVE

#include "SDLInput.h"
#include "SDLDisplay.h" // for signalQuit()
#include "mock/Arduino.h"

// ─────────────────────────────────────────────────────────────────────────────
SDLInput::SDLInput(SDLDisplay* display)
    : _display(display), _lastActivityMs(millis()), _timeoutSec(0)
{}

// ─────────────────────────────────────────────────────────────────────────────
ButtonEvent SDLInput::pollInput()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            if (_display) _display->signalQuit();
            return ButtonEvent::QUIT;

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

    if (_timeoutSec > 0 && _lastActivityMs > 0 && (millis() - _lastActivityMs > _timeoutSec * 1000UL)) {
        _lastActivityMs = millis();
        return ButtonEvent::SLEEP;
    }

    return ButtonEvent::NONE;
}

#endif // PLATFORM_NATIVE
