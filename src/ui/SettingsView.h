// EENK — SettingsView
// Multi-page settings menu for the Xteink X4 e-reader.
//
// Pages (4 total):
//   0  Reading    — story font, choice font, margin, partial-refresh interval
//   1  Behaviour  — sleep/save timeout, full-refresh interval
//   2  Input      — annotated button-mapping diagram (cycles with LEFT/RIGHT)
//   3  Danger Zone — delete save, delete story, format SD
//
// Navigation:
//   UP / DOWN      — move between items on the current page
//   LEFT / RIGHT   — change the selected item's value (or navigate pages
//                    when _itemIndex == 0 and at the edge)
//   BACK           — go to previous page, or exit (page 0)
//   CONFIRM        — used for dangerous actions
#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"
#include "FooterWidget.h"

class BatteryWidget;

class SettingsView {
public:
    SettingsView(IDisplay& display, IInput& input,
                 BatteryWidget& battery, AppSettings& settings);
    ~SettingsView();

    // Run the settings UI loop. Blocks until the user exits (BACK on page 0).
    // Saves settings before returning.
    void run();

private:
    IDisplay&      _display;
    IInput&        _input;
    BatteryWidget& _battery;
    AppSettings&   _settings;

    int  _pageIndex = 0;   // 0=Reading, 1=Behaviour, 2=Input, 3=Danger Zone
    int  _itemIndex = 0;   // selected row within page
    bool _dirty     = false; // settings changed, need save

    static constexpr int STATUS_BAR_H = 28;
    static constexpr int PAGE_COUNT   = 4;

    void renderPage();
    void renderStatusBar(const char* pageTitle);
    void renderFooter();
    void renderReadingPage();
    void renderBehaviourPage();
    void renderInputPage();     // annotated device diagram
    void renderDangerPage();

    // Input handlers per page
    void handleReadingInput(ButtonEvent ev);
    void handleBehaviourInput(ButtonEvent ev);
    void handleInputPageInput(ButtonEvent ev);
    void handleDangerInput(ButtonEvent ev);

    // Shared helper: draw a settings row
    // label: left-side label, value: right-side current value string
    // selected: highlight this row
    void drawSettingsRow(int y, const char* label, const char* value, bool selected);

    // Annotated device-body diagram for the Input Mapping page.
    // layoutIndex selects which action labels to show.
    void drawDeviceDiagram(int x, int y, int width, int height, int layoutIndex);

    // Danger zone actions
    void deleteSave();
    void deleteStory();
    void formatSD();
};

