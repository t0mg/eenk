#pragma once
/**
 * eenk — IDisplay: Platform-agnostic display interface
 *
 * Implemented by:
 *   SDLDisplay        (PLATFORM_NATIVE)              — SDL2 800×480 software-rendered window
 *   EspEinkDisplay    (PLATFORM_ESP32)               — SSD1677 driver via Papyrix GfxRenderer
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

    /** Display width in pixels (or terminal columns for serial mode). */
    virtual int getWidth()  const = 0;
    /** Display height in pixels (or a large number for serial mode). */
    virtual int getHeight() const = 0;

    /**
     * Get the underlying pixel-based GfxRenderer for drawing proportional
     * fonts and graphics. Returns nullptr for text-only displays (e.g. serial).
     */
    virtual GfxRenderer* getRenderer() = 0;

    // ── Text-mode hooks (optional, default no-op) ─────────────────────────────
    // Called by InkEngine::redraw() when getRenderer() returns nullptr.
    // Override these in text-only display implementations.

    /** Draw a single narrative text line. */
    virtual void drawNarrativeLine(const char* /*text*/) {}
    /** Draw a single choice line. 'selected' highlights the current cursor. */
    virtual void drawChoiceLine(int /*index*/, const char* /*text*/, bool /*selected*/) {}
    /** Draw a visual separator between narrative and choices. */
    virtual void drawSeparator() {}
};
