#pragma once
#ifdef PLATFORM_NATIVE

#include "hal/IInput.h"
#include <SDL.h>

/**
 * EENK — SDLInput
 *
 * SDL2 implementation of IInput.
 * Pumps SDL events and maps keyboard keys to the ButtonEvent enum.
 *
 * Key mapping:
 *   UP     ← Arrow-Up    / W / K
 *   DOWN   ← Arrow-Down  / S / J
 *   LEFT   ← Arrow-Left  / A / H
 *   RIGHT  ← Arrow-Right / D / L
 *   CONFIRM← Return / Space
 *   BACK   ← Escape / Backspace
 *   QUIT   ← SDL_QUIT event (window X button)
 */
class SDLDisplay; // forward declaration — SDLInput shares the quit flag

class SDLInput : public IInput
{
public:
    /**
     * @param display Pointer to SDLDisplay so we can forward the QUIT event.
     *                May be nullptr if you don't need QUIT propagation.
     */
    explicit SDLInput(SDLDisplay* display = nullptr);
    ~SDLInput() override = default;

    /**
     * Non-blocking poll. Processes all pending SDL events this frame and
     * returns the first meaningful button event found (or NONE).
     */
    ButtonEvent pollInput() override;
    uint32_t getLastActivityTime() const override { return _lastActivityMs; }
    void setAutoSleepTimeout(uint16_t seconds) override { _timeoutSec = seconds; }

private:
    class SDLDisplay* _display;
    uint32_t _lastActivityMs;
    uint16_t _timeoutSec;
};

#endif // PLATFORM_NATIVE
