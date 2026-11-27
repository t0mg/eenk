#ifdef PLATFORM_ESP32

#include "HalInit.h"
#include "os/BootManager.h"
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <esp_system.h>
#include <InputManager.h>
#include <SD.h>
#include <SPI.h>
#include <PowerManager.h>
#include <XteinkDetect.h>

namespace HalInit {

namespace {
// Set when UP is held on a cold boot; main.cpp launches the in-app SD firmware
// updater in recovery mode after display/input/SD init.
bool s_recoveryRequested = false;
} // namespace

bool recoveryRequested() { return s_recoveryRequested; }

void earlyBootCheck() {
  initHardware();

  // Only cold boots / wakes may enter SD recovery. A warm ESP.restart() (e.g.
  // post-flash reboot, story launch, or crash) must boot normally even if UP
  // is still held, preventing a recovery re-entry loop.
  const esp_reset_reason_t resetReason = esp_reset_reason();
  if (resetReason != ESP_RST_POWERON && resetReason != ESP_RST_DEEPSLEEP &&
      resetReason != ESP_RST_EXT) {
    Serial.printf("[Boot] Warm boot (reset reason %d): skipping SD recovery check.\n",
                  (int)resetReason);
    return;
  }

  InputManager im;
  im.begin();
  int pressedCount = 0;
  for (int i = 0; i < 5; i++) {
    im.update();
    if (im.isPressed(InputManager::BTN_UP)) {
      pressedCount++;
    }
    delay(10);
  }

  if (pressedCount >= 4) {
    Serial.println("[Boot] UP button held. Requesting SD recovery updater.");
    s_recoveryRequested = true;
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
  return SD.begin(12, SPI, 40000000, "/sd", 8);
}

void prepareForSleep() {
  freeink::PowerManager::armPowerButtonWakeup();
}

BatteryMonitor *createBatteryMonitor() { return new BatteryMonitor(); }

IFrontlight *createFrontlight() { return nullptr; }

// Use Arduino's calibrated analogReadMilliVolts() default on ESP32-C3 so eFuse
// calibration applies (uncalibrated raw*3100/4095 reads high and pins X4 at 100%).

} // namespace HalInit

#endif // PLATFORM_ESP32
