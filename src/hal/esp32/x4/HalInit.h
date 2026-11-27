#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IFrontlight.h"
class BatteryMonitor;

namespace HalInit {
    void earlyBootCheck();
    // True when UP was held at cold boot (Up+Power SD recovery).
    bool recoveryRequested();
    void initHardware();
    bool mountSdForUpdater();
    void prepareForSleep();
    BatteryMonitor* createBatteryMonitor();
    IFrontlight* createFrontlight();
}

#endif // PLATFORM_ESP32
