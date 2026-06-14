#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IDisplay.h"

/**
 * EENK — EspSerialDisplay
 *
 * A debug IDisplay implementation that renders the game state to the ESP32
 * Serial port using VT100 escape codes. No e-ink hardware required.
 *
 * Enable via the [env:esp32c3_serial] PlatformIO environment (SERIAL_DEBUG flag).
 */
class EspSerialDisplay : public IDisplay
{
public:
    EspSerialDisplay();
    ~EspSerialDisplay() override = default;

    void clear() override;
    void present() override;
    void fullRefresh() override {}

    int getWidth()  const override { return 80; }   // 80-char terminal width
    int getHeight() const override { return 2000; } // effectively unlimited

    // Returns nullptr — serial rendering is handled via drawNarrativeLine/drawChoiceLine
    GfxRenderer* getRenderer() override { return nullptr; }

    // IDisplay text-mode hooks (called by InkEngine when renderer is null)
    void drawNarrativeLine(const char* text) override;
    void drawChoiceLine(int index, const char* text, bool selected) override;
    void drawSeparator() override;
};

#endif // PLATFORM_ESP32
