#include "SystemUI.h"
#include <cstdio>
#include <cstring>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>
#include <GfxRenderer.h> // from Papyrix

static EpdFont sysFontNormal(&ui_12);
static EpdFontFamily sysFamilyNormal(&sysFontNormal);

static EpdFont sysFontBold(&ui_bold_12);
static EpdFontFamily sysFamilyBold(&sysFontBold);
#endif

SystemUI::SystemUI(IDisplay& display) : _display(display)
{
}

SystemUI::~SystemUI()
{
}

void SystemUI::ensureFonts()
{
    if (_fontsLoaded) return;
    
#ifdef PLATFORM_ESP32
    auto renderer = _display.getRenderer();
    if (renderer) {
        // ID 10 = Normal UI font, ID 11 = Bold UI font (to avoid clashing with InkEngine's 0 and 1)
        renderer->insertFont(10, sysFamilyNormal);
        renderer->insertFont(11, sysFamilyBold);
    }
#endif
    
    _fontsLoaded = true;
}

void SystemUI::showError(const char* title, const char* message)
{
    ensureFonts();
    
    auto renderer = _display.getRenderer();
    if (renderer) {
        _display.clear();
#ifdef PLATFORM_ESP32
        renderer->drawText(11, 50, 50, title);
        renderer->drawText(10, 50, 90, message);
#else
        // Fallback for native
        renderer->drawText(0, 50, 50, title);
        renderer->drawText(0, 50, 90, message);
#endif
        _display.fullRefresh();
    } else {
        printf("=== %s ===\n%s\n", title, message);
    }
}

void SystemUI::showLoading(const char* title, float progress)
{
    ensureFonts();

    auto renderer = _display.getRenderer();
    if (renderer) {
        _display.clear();
        
        // Draw progress bar
        int barWidth = _display.getWidth() - 100;
        int barHeight = 20;
        int barX = 50;
        int barY = (_display.getHeight() - barHeight) / 2;
        
#ifdef PLATFORM_ESP32
        renderer->drawText(11, barX, barY - 40, title);
        
        // Frame
        renderer->drawRect(barX, barY, barWidth, barHeight);
        // Fill
        int fillWidth = (int)(barWidth * progress);
        if (fillWidth > 0) {
            renderer->fillRect(barX, barY, fillWidth, barHeight);
        }
#endif
        
        // For loading, partial refresh is better so it animates cleanly
        _display.present();
    } else {
        printf("\r%s: %d%%", title, (int)(progress * 100));
        fflush(stdout);
    }
}

void SystemUI::showSleepCover()
{
    ensureFonts();

    auto renderer = _display.getRenderer();
    if (renderer) {
        _display.clear();
        
#ifdef PLATFORM_ESP32
        const char* splash = "E E N K";
        const char* msg = "Powered Off";
        
        // Placed at top-left to avoid right-edge clipping
        renderer->drawText(11, 50, 50, splash);
        renderer->drawText(10, 50, 90, msg);
#endif

        _display.fullRefresh();
    } else {
        printf("\nDevice sleeping...\n");
    }
}
