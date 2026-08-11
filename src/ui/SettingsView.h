// eenk — SettingsView
// Single-panel settings menu for the Xteink X4 e-reader.
//
// Navigation:
//   UP / DOWN      — move between items
//   LEFT / RIGHT   — change the selected item's value
//   CONFIRM        — toggle setting / actuate selected action
//   BACK / QUIT    — exit settings
#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"
#include "FooterWidget.h"
#include "HeaderWidget.h"
#include "hal/IFrontlight.h"
#include "os/SdFontCatalogue.h"

class BatteryWidget;

class SettingsView {
public:
    SettingsView(IDisplay& display, IInput& input,
                 BatteryWidget& battery, IFrontlight* frontlight, AppSettings& settings);
    ~SettingsView();

    // Run the settings UI loop. Blocks until the user exits (BACK/QUIT).
    // Saves settings before returning.
    void run();

private:
    IDisplay&      _display;
    IInput&        _input;
    BatteryWidget& _battery;
    IFrontlight*   _frontlight;
    AppSettings&   _settings;

    int  _itemIndex = 0;   // selected row (0–8)
    bool _dirty     = false;
    SdFontCatalogue _fontCatalogue;
    int _currentFontIndex = 0;
    static constexpr int DISP_H = 800;

    void renderPage();
    void renderFooter();
    void handleInput(ButtonEvent ev);

    // Shared helper: draw a settings row
    // label: left-side label, value: right-side current value string
    // selected: highlight this row
    void drawSettingsRow(int y, const char* label, const char* value, bool selected);

    // Danger zone actions
    void formatSD();
};
