#pragma once
#ifdef PLATFORM_NATIVE

#include "hal/IDisplay.h"
#include <SDL.h>
#include <EInkDisplay.h>
#include <GfxRenderer.h>

/**
 * EENK — SDLDisplay
 *
 * SDL2 implementation of IDisplay.
 * Creates an 480x800 software-rendered window simulating the Xteink X4 e-ink panel.
 */
class SDLDisplay : public IDisplay
{
public:
    // Window dimensions
    static constexpr int WIN_W = 480;
    static constexpr int WIN_H = 800;

    SDLDisplay();
    ~SDLDisplay() override;

    void clear() override;
    void present() override;
    void fullRefresh() override;

    int getWidth()  const override { return WIN_W; }
    int getHeight() const override { return WIN_H; }

    GfxRenderer* getRenderer() override { return &_gfxRenderer; }

    /** True if the SDL window has been asked to close. */
    bool shouldQuit() const { return _quit; }
    /** Called by SDLInput when SDL_QUIT is received. */
    void signalQuit()       { _quit = true; }

private:
    SDL_Window*   _window   = nullptr;
    SDL_Renderer* _renderer = nullptr;
    SDL_Texture*  _texture  = nullptr;
    bool          _quit     = false;

    EInkDisplay  _mockEink;
    GfxRenderer  _gfxRenderer;
};

#endif // PLATFORM_NATIVE
