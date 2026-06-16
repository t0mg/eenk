#pragma once
/**
 * EENK — IInput: Platform-agnostic input interface
 *
 * Implemented by:
 *   SDLInput   (PLATFORM_NATIVE)   — keyboard mapping via SDL_PollEvent
 *   ADCInput   (PLATFORM_ESP32)    — resistor-ladder ADC polling (M2)
 */

/** All possible button events the game engine cares about. */
enum class ButtonEvent
{
    NONE,      ///< No input this frame
    UP,        ///< Navigate up / previous choice
    DOWN,      ///< Navigate down / next choice
    LEFT,      ///< Scroll left / page back
    RIGHT,     ///< Scroll right / page forward
    CONFIRM,   ///< Select / advance narrative
    BACK,      ///< Save / exit / cancel
    SLEEP,     ///< Device power off / sleep requested
    QUIT,      ///< Platform quit (window close, power button)
};

class IInput
{
public:
    virtual ~IInput() = default;

    /**
     * Non-blocking poll for the next button event.
     * Returns ButtonEvent::NONE if no key/button was pressed since the last call.
     */
    virtual ButtonEvent pollInput() = 0;
};
