#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IFrontlight.h"
class BatteryMonitor;

namespace HalInit {
    void earlyBootCheck();
    // Locked builds: true when UP was held at boot (Up+Power SD recovery).
    // Always false on stock builds.
    bool recoveryRequested();
    void initHardware();
    bool mountSdForUpdater();
    void prepareForSleep();
    BatteryMonitor* createBatteryMonitor();
    IFrontlight* createFrontlight();
}

#endif // PLATFORM_ESP32
