#pragma once

#include "hal/IInput.h"

class InkEngine;
class InkDisplayManager;
class InkStoryManager;
class IFrontlight;
class BatteryWidget;

class InkInputHandler {
public:
    InkInputHandler(IInput& input);
    ~InkInputHandler();

    void tickWaitingInput(InkEngine& engine, InkDisplayManager& display, InkStoryManager& story, class IDisplay& IDisplay, struct AppSettings& settings, IFrontlight* frontlight, BatteryWidget* batteryWidget);
    void tickStoryEnded(InkEngine& engine, InkDisplayManager& display, InkStoryManager& story, class IDisplay& IDisplay, struct AppSettings& settings, IFrontlight* frontlight, BatteryWidget* batteryWidget);

private:
    IInput& _input;

    void handleScroll(InkDisplayManager& display, int scrollAmount);
    void handleCommonNavigation(bool isStoryEnded, InkEngine& engine, InkDisplayManager& display, InkStoryManager& story, class IDisplay& IDisplay, struct AppSettings& settings, IFrontlight* frontlight, BatteryWidget* batteryWidget);
};
