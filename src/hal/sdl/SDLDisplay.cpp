#ifdef PLATFORM_NATIVE

#include "SDLDisplay.h"
#include <cstdio>
#include <cstring>
#include <algorithm>


// Colour palette — warm paper-white e-ink simulation
static constexpr SDL_Color COLOR_BG  = {0xF5, 0xF1, 0xE8, 0xFF}; // parchment white
static constexpr SDL_Color COLOR_FG  = {0x1A, 0x1A, 0x1A, 0xFF}; // near-black ink

// ─────────────────────────────────────────────────────────────────────────────
SDLDisplay::SDLDisplay(int winW, int winH)
    : _winW(winW), _winH(winH), _mockEink(winH, winW), _gfxRenderer(_mockEink)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[SDLDisplay] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    _window = SDL_CreateWindow(
        "eenk — Interactive Fiction Runtime (E-Ink Simulator)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        _winW, _winH,
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

    // ARGB8888 texture to blast our 1bpp buffer into
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, _winW, _winH);
    if (!_texture) {
        fprintf(stderr, "[SDLDisplay] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }

    // Nearest-neighbour scaling keeps pixels crisp
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    // Set logical size so the window can be resized without affecting layout
    SDL_RenderSetLogicalSize(_renderer, _winW, _winH);

    _gfxRenderer.begin();
    // Default orientation
    _gfxRenderer.setOrientation(GfxRenderer::PortraitInverted);

    clear();
    present();
    printf("[SDLDisplay] Window ready — %d×%d (1-bpp simulation)\n", _winW, _winH);
}

// ─────────────────────────────────────────────────────────────────────────────
SDLDisplay::~SDLDisplay()
{
    if (_texture)  SDL_DestroyTexture(_texture);
    if (_renderer) SDL_DestroyRenderer(_renderer);
    if (_window)   SDL_DestroyWindow(_window);
    SDL_Quit();
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::clear()
{
    _gfxRenderer.clearScreen();
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::present()
{
    if (!_texture || !_renderer) return;

    uint32_t* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(_texture, nullptr, (void**)&pixels, &pitch) == 0) {
        // Fast hardcoded pixel packing
        uint32_t bgPixel = SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        uint32_t fgPixel = SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), COLOR_FG.r, COLOR_FG.g, COLOR_FG.b, COLOR_FG.a);
        
        uint8_t* fb = _mockEink.getFrameBuffer();
        int panelW = _mockEink.getDisplayWidth();
        int panelWBytes = _mockEink.getDisplayWidthBytes();
        
        for (int y = 0; y < _winH; ++y) {
            for (int x = 0; x < _winW; ++x) {
                int hw_x = (panelW - 1) - y;
                int hw_y = x;
                uint32_t idx = hw_y * panelWBytes + (hw_x / 8);
                uint8_t bit = 7 - (hw_x % 8);
                bool isWhite = (fb[idx] & (1 << bit)) != 0;
                
                pixels[y * (pitch / 4) + x] = isWhite ? bgPixel : fgPixel;
            }
        }
        SDL_UnlockTexture(_texture);
    }

    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
}

// ─────────────────────────────────────────────────────────────────────────────
void SDLDisplay::fullRefresh()
{
    present();
}

#endif // PLATFORM_NATIVE
