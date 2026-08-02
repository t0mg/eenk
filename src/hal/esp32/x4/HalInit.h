#pragma once
#ifdef PLATFORM_ESP32

class BatteryMonitor;

namespace HalInit {
    void earlyBootCheck();
    void initHardware();
    bool mountSdForUpdater();
    void prepareForSleep();
    BatteryMonitor* createBatteryMonitor();
}

#endif // PLATFORM_ESP32
