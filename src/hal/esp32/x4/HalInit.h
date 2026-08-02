#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IFrontlight.h"
class BatteryMonitor;

namespace HalInit {
    void earlyBootCheck();
    void initHardware();
    bool mountSdForUpdater();
    void prepareForSleep();
    BatteryMonitor* createBatteryMonitor();
    IFrontlight* createFrontlight();
}

#endif // PLATFORM_ESP32
