#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IDisplay.h"
#include <EInkDisplay.h>
#include <GfxRenderer.h>

class EspEinkDisplay : public IDisplay
{
public:
    EspEinkDisplay();
    ~EspEinkDisplay() override = default;

    void clear() override;
    void present() override;
    void fullRefresh() override;

    int getWidth() const override { return WIN_W; }
    int getHeight() const override { return WIN_H; }

    GfxRenderer* getRenderer() override { return &_gfxRenderer; }

private:
    EInkDisplay _eink;
    GfxRenderer _gfxRenderer;

    static constexpr int WIN_W = 480;
    static constexpr int WIN_H = 800;
};

#endif // PLATFORM_ESP32
