#ifdef PLATFORM_ESP32

#include "HalInit.h"
#include "EspFrontlight.h"
#include "EspSdmmcStorage.h"
#include "os/BootManager.h"
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <PowerManager.h>
#include <driver/rtc_io.h>
#include <esp_system.h>

namespace HalInit {

namespace {
bool s_recoveryRequested = false;
} // namespace

bool recoveryRequested() { return s_recoveryRequested; }

void initHardware() {
  // Power rails on X4 Pro:
  // GPIO1: peripheral rail (OUTPUT HIGH)
  // GPIO2: touch power enable (OUTPUT LOW, active-low)
  // GPIO5: SD power enable (OUTPUT LOW, active-low)
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
}

bool mountSdForUpdater() {
  static EspSdmmcStorage sdmmc;
  return sdmmc.begin();
}

void earlyBootCheck() {
  initHardware();

  // Only cold boots / wakes may enter SD recovery. A warm ESP.restart()
  // (e.g. post-flash reboot, story launch, or crash) must boot normally.
  const esp_reset_reason_t resetReason = esp_reset_reason();
  if (resetReason != ESP_RST_POWERON && resetReason != ESP_RST_DEEPSLEEP &&
      resetReason != ESP_RST_EXT) {
    Serial.printf("[Boot] Warm boot (reset reason %d): skipping SD recovery check.\n",
                  (int)resetReason);
    return;
  }

  // Check GPIO0 (Left button) held continuously across 5 samples
  pinMode(0, INPUT_PULLUP);
  int pressedCount = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(0) == LOW) {
      pressedCount++;
    }
    delay(10);
  }

  if (pressedCount >= 4) {
    Serial.println("[Boot] LEFT button (GPIO0) held. Requesting SD recovery updater.");
    s_recoveryRequested = true;
  }
}

void prepareForSleep() {
  // Drive power rails off for sleep
  digitalWrite(2, HIGH); // Touch power off
  digitalWrite(5, HIGH); // SD power off

  freeink::PowerManager::armPowerButtonWakeup();
}

BatteryMonitor *createBatteryMonitor() { return new BatteryMonitor(); }

IFrontlight *createFrontlight() { return new EspFrontlight(); }

} // namespace HalInit

#endif // PLATFORM_ESP32
