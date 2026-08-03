#ifdef PLATFORM_ESP32

#include "HalInit.h"
#include "os/BootManager.h"
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>
#include <SD.h>
#include <SPI.h>
#include <esp_sleep.h>

namespace HalInit {

void earlyBootCheck() {
    bool updaterMode = false;

    // Check UP button
    InputManager im;
    im.begin();
    im.update();
    if (im.isPressed(InputManager::BTN_UP)) {
        Serial.println("[Boot] UP button held. Forcing updater mode.");
        updaterMode = true;
    }

    // Check SD card for /firmware.bin
    if (!updaterMode) {
        // Init SPI first so SD.begin() works (SCLK=8, MISO=7, MOSI=10, CS=21)
        SPI.begin(8, 7, 10, 21);
        if (SD.begin(12, SPI, 40000000)) {
            if (SD.exists("/firmware.bin")) {
                Serial.println("[Boot] /firmware.bin found on SD. Forcing updater mode.");
                updaterMode = true;
            }
        }
    }

    if (updaterMode) {
        Serial.println("[Boot] Restarting into Updater (app1)...");
        BootManager::bootUpdater();
        // Will reboot
    }
}

void initHardware() {
    // No special power rails on X4
}

bool mountSdForUpdater() {
    // EspEinkDisplay already called SPI.begin(8, 7, 10, 21)
    return SD.begin(12, SPI, 40000000);
}

void prepareForSleep() {
    disableGpioPullsForSleep();
    esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
}

BatteryMonitor* createBatteryMonitor() {
    return new BatteryMonitor(8 /*GPIO8*/);
}

IFrontlight* createFrontlight() {
    return nullptr;
}

} // namespace HalInit

#endif // PLATFORM_ESP32
