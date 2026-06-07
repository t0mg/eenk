#pragma once
/**
 * EENK — IDisplay: Platform-agnostic display interface
 *
 * Implemented by:
 *   SDLDisplay   (PLATFORM_NATIVE)   — SDL2 800×480 software-rendered window
 *   EInkDisplay  (PLATFORM_ESP32)    — SSD1677 driver via Papyrix GfxRenderer (M2)
 */

class GfxRenderer;

class IDisplay
{
public:
    virtual ~IDisplay() = default;

    /** Clear the entire framebuffer to the background colour. */
    virtual void clear() = 0;


    /**
     * Present changes to the display.
     * On ESP32 this triggers a partial SSD1677 refresh.
     * On native this calls SDL_RenderPresent().
     */
    virtual void present() = 0;

    /** Force a complete display refresh (ghosting mitigation on real e-ink). */
    virtual void fullRefresh() = 0;

    /** Display width in pixels. */
    virtual int getWidth()  const = 0;
    /** Display height in pixels. */
    virtual int getHeight() const = 0;
    /**
     * Get the underlying pixel-based GfxRenderer for drawing proportional
     * fonts and graphics.
     */
    virtual GfxRenderer* getRenderer() = 0;
};
