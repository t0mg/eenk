#ifdef PLATFORM_NATIVE

#include "SDLInput.h"
#include "SDLDisplay.h" // for signalQuit()

// ─────────────────────────────────────────────────────────────────────────────
SDLInput::SDLInput(SDLDisplay* display)
    : _display(display)
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
                return ButtonEvent::UP;
            // DOWN
            case SDLK_DOWN:  case SDLK_s: case SDLK_j:
                return ButtonEvent::DOWN;
            // LEFT
            case SDLK_LEFT:  case SDLK_a: case SDLK_h:
                return ButtonEvent::LEFT;
            // RIGHT
            case SDLK_RIGHT: case SDLK_d: case SDLK_l:
                return ButtonEvent::RIGHT;
            // CONFIRM
            case SDLK_RETURN: case SDLK_RETURN2: case SDLK_SPACE:
                return ButtonEvent::CONFIRM;
            // BACK / QUIT
            case SDLK_ESCAPE: case SDLK_BACKSPACE:
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
    return ButtonEvent::NONE;
}

#endif // PLATFORM_NATIVE
