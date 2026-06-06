#pragma once
#ifdef PLATFORM_NATIVE

#include "hal/IDisplay.h"
#include <SDL.h>

/**
 * EENK — SDLDisplay
 *
 * SDL2 implementation of IDisplay.
 * Creates an 800×480 software-rendered window simulating the Xteink X4 e-ink panel.
 *
 * Colour palette (warm paper-white, ink-black — matches e-ink feel):
 *   Background: #F5F1E8
 *   Foreground: #1A1A1A
 *
 * Font: bundled 8×8 CP437 bitmap, rendered at 2× scale → 16×16 px per glyph.
 * Grid: 50 columns × 30 rows at 800×480.
 */
class SDLDisplay : public IDisplay
{
public:
    // Rendering constants
    static constexpr int FONT_SIZE  = 8;   // source glyph size
    static constexpr int FONT_SCALE = 2;   // render scale factor
    static constexpr int GLYPH_W   = FONT_SIZE * FONT_SCALE;  // 16 px
    static constexpr int GLYPH_H   = FONT_SIZE * FONT_SCALE;  // 16 px
    static constexpr int LEADING    = 2;   // extra px between rows
    static constexpr int LINE_H     = GLYPH_H + LEADING;      // 18 px
    static constexpr int PAD_X      = 8;   // left/right margin in px
    static constexpr int PAD_Y      = 8;   // top/bottom margin in px

    // Window dimensions
    static constexpr int WIN_W = 800;
    static constexpr int WIN_H = 480;

    // Character grid
    static constexpr int COLS = (WIN_W - 2 * PAD_X) / GLYPH_W;  // ~48 cols
    static constexpr int ROWS = (WIN_H - 2 * PAD_Y) / LINE_H;   // ~25 rows

    SDLDisplay();
    ~SDLDisplay() override;

    void clear()                                              override;
    void drawText(int col, int row, const char* text,
                  bool inverted = false)                      override;
    void drawHLine(int y)                                     override;
    void present()                                            override;
    void fullRefresh()                                        override;

    int getWidth()      const override { return WIN_W; }
    int getHeight()     const override { return WIN_H; }
    int getLineHeight() const override { return LINE_H; }
    int getCharWidth()  const override { return GLYPH_W; }
    int getCols()       const override { return COLS; }
    int getRows()       const override { return ROWS; }

    /** True if the SDL window has been asked to close. */
    bool shouldQuit() const { return _quit; }
    /** Called by SDLInput when SDL_QUIT is received. */
    void signalQuit()       { _quit = true; }

private:
    SDL_Window*   _window   = nullptr;
    SDL_Renderer* _renderer = nullptr;
    bool          _quit     = false;

    /** Draw a single glyph at pixel position (px, py). */
    void drawGlyph(int px, int py, char c, bool inverted);
};

#endif // PLATFORM_NATIVE
