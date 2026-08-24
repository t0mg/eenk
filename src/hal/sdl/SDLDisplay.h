#pragma once
#ifdef PLATFORM_NATIVE

#include "hal/IDisplay.h"
#include <SDL.h>
#include <EInkDisplay.h>
#include <GfxRenderer.h>

/**
 * eenk — SDLDisplay
 *
 * SDL2 implementation of IDisplay.
 * Creates an 480x800 software-rendered window simulating the Xteink X4 e-ink panel.
 */
class SDLDisplay : public IDisplay
{
public:
    // Window dimensions (default X4: 480x800)
    static constexpr int WIN_W = 480;
    static constexpr int WIN_H = 800;
    static constexpr int X3_WIN_W = 528;
    static constexpr int X3_WIN_H = 792;

    explicit SDLDisplay(int winW = WIN_W, int winH = WIN_H);
    ~SDLDisplay() override;

    void clear() override;
    void present() override;
    void fullRefresh() override;

    int getWidth()  const override { return _winW; }
    int getHeight() const override { return _winH; }

    GfxRenderer* getRenderer() override { return &_gfxRenderer; }

    /** True if the SDL window has been asked to close. */
    bool shouldQuit() const { return _quit; }
    /** Called by SDLInput when SDL_QUIT is received. */
    void signalQuit()       { _quit = true; }

private:
    int           _winW     = WIN_W;
    int           _winH     = WIN_H;
    SDL_Window*   _window   = nullptr;
    SDL_Renderer* _renderer = nullptr;
    SDL_Texture*  _texture  = nullptr;
    bool          _quit     = false;

    EInkDisplay  _mockEink;
    GfxRenderer  _gfxRenderer;
};

#endif // PLATFORM_NATIVE
