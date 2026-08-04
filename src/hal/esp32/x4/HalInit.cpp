#ifdef PLATFORM_ESP32

#include "HalInit.h"
#include "os/BootManager.h"
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>
#include <SD.h>
#include <SPI.h>
#include <PowerManager.h>
#include <XteinkDetect.h>

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
        Serial.println(
            "[Boot] /firmware.bin found on SD. Forcing updater mode.");
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
  bool isX3 = freeink::selectXteinkDevice();
  if (isX3) {
    Serial.println("[HalInit] Xteink X3 detected and profile selected.");
  } else {
    Serial.println("[HalInit] Xteink X4 detected and profile selected.");
  }
}

bool mountSdForUpdater() {
  // EspEinkDisplay already called SPI.begin(8, 7, 10, 21)
  return SD.begin(12, SPI, 40000000);
}

void prepareForSleep() {
  freeink::PowerManager::armPowerButtonWakeup();
}

BatteryMonitor *createBatteryMonitor() { return new BatteryMonitor(); }

IFrontlight *createFrontlight() { return nullptr; }

} // namespace HalInit

#endif // PLATFORM_ESP32
