#ifdef PLATFORM_NATIVE

#include "SDLDisplay.h"
#include "font_8x8.h"
#include <cstdio>
#include <cstring>

// Colour palette — warm paper-white e-ink simulation
static constexpr SDL_Color COLOR_BG  = {0xF5, 0xF1, 0xE8, 0xFF}; // parchment white
static constexpr SDL_Color COLOR_FG  = {0x1A, 0x1A, 0x1A, 0xFF}; // near-black ink
static constexpr SDL_Color COLOR_DIM = {0x8A, 0x87, 0x80, 0xFF}; // divider / shadow

// ─────────────────────────────────────────────────────────────────────────────
SDLDisplay::SDLDisplay()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[SDLDisplay] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    _window = SDL_CreateWindow(
        "EENK — Interactive Fiction Runtime",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!_window) {
        fprintf(stderr, "[SDLDisplay] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    _renderer = SDL_CreateRenderer(_window, -1,
        SDL_RENDERER_SOFTWARE);
    if (!_renderer) {
        fprintf(stderr, "[SDLDisplay] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }

    // Nearest-neighbour scaling keeps pixels crisp
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    // Set logical size so the window can be resized without affecting layout
    SDL_RenderSetLogicalSize(_renderer, WIN_W, WIN_H);

    clear();
    present();
    printf("[SDLDisplay] Window ready — %d×%d, grid %d×%d\n",
           WIN_W, WIN_H, COLS, ROWS);
}

// ─────────────────────────────────────────────────────────────────────────────
SDLDisplay::~SDLDisplay()
{
    if (_renderer) SDL_DestroyRenderer(_renderer);
    if (_window)   SDL_DestroyWindow(_window);
    SDL_Quit();
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::clear()
{
    if (!_renderer) return;
    SDL_SetRenderDrawColor(_renderer,
        COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
    SDL_RenderClear(_renderer);
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::drawGlyph(int px, int py, char c, bool inverted)
{
    const uint8_t* glyph = fontGlyph(c);
    SDL_Color fg = inverted ? COLOR_BG : COLOR_FG;
    SDL_Color bg = inverted ? COLOR_FG : COLOR_BG;

    // Fill glyph background first (for inverted / highlight)
    if (inverted) {
        SDL_SetRenderDrawColor(_renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_Rect rect = { px, py, GLYPH_W, GLYPH_H };
        SDL_RenderFillRect(_renderer, &rect);
    }

    SDL_SetRenderDrawColor(_renderer, fg.r, fg.g, fg.b, fg.a);

    // Render each source pixel at 2× scale
    for (int row = 0; row < FONT_SIZE; ++row) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_SIZE; ++col) {
            if (bits & (1 << col)) {
                SDL_Rect pixel = {
                    px + col * FONT_SCALE,
                    py + row * FONT_SCALE,
                    FONT_SCALE,
                    FONT_SCALE
                };
                SDL_RenderFillRect(_renderer, &pixel);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::drawText(int col, int row, const char* text, bool inverted)
{
    if (!_renderer || !text) return;

    int px = PAD_X + col * GLYPH_W;
    int py = PAD_Y + row * LINE_H;
    int curCol = col;

    for (const char* p = text; *p; ++p) {
        if (curCol >= COLS) break; // clip at display edge
        drawGlyph(px, py, *p, inverted);
        px += GLYPH_W;
        ++curCol;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::drawHLine(int y)
{
    if (!_renderer) return;
    SDL_SetRenderDrawColor(_renderer,
        COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(_renderer, PAD_X, y, WIN_W - PAD_X, y);
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::present()
{
    if (_renderer) SDL_RenderPresent(_renderer);
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::fullRefresh()
{
    // On SDL there's no e-ink ghosting, but we still clear + re-present
    // to honour the interface contract (useful when adding partial-update
    // tracking later).
    clear();
    present();
}

#endif // PLATFORM_NATIVE
