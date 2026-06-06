#pragma once
/**
 * EENK — IDisplay: Platform-agnostic display interface
 *
 * Implemented by:
 *   SDLDisplay   (PLATFORM_NATIVE)   — SDL2 800×480 software-rendered window
 *   EInkDisplay  (PLATFORM_ESP32)    — SSD1677 driver via Papyrix GfxRenderer (M2)
 */

class IDisplay
{
public:
    virtual ~IDisplay() = default;

    /** Clear the entire framebuffer to the background colour. */
    virtual void clear() = 0;

    /**
     * Draw a null-terminated string at character-grid position (col, row).
     * @param col  Column index (0-based, in character widths)
     * @param row  Row index (0-based, in line heights)
     * @param text Null-terminated UTF-8 string (ASCII subset only for now)
     * @param inverted If true, swap fg/bg colours (for choice highlighting)
     */
    virtual void drawText(int col, int row, const char* text, bool inverted = false) = 0;

    /**
     * Draw a horizontal divider line at the given pixel y-coordinate.
     * Used to visually separate narrative text from the choice list.
     */
    virtual void drawHLine(int y) = 0;

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
    /** Height of one text row in pixels (font height + leading). */
    virtual int getLineHeight() const = 0;
    /** Width of one character in pixels. */
    virtual int getCharWidth()  const = 0;
    /** Number of usable character columns. */
    virtual int getCols() const = 0;
    /** Number of usable character rows. */
    virtual int getRows() const = 0;
};
