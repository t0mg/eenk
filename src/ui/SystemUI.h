#pragma once
#include "hal/IDisplay.h"

class SystemUI
{
public:
    SystemUI(IDisplay& display);
    ~SystemUI();

    void showError(const char* title, const char* message);
    void showLoading(const char* title, float progress);
    void showSleepCover();

private:
    IDisplay& _display;
    bool _fontsLoaded = false;

    void ensureFonts();
};
