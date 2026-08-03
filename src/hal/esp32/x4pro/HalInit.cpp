#ifdef PLATFORM_ESP32

#include "HalInit.h"
#include "EspFrontlight.h"
#include "EspSdmmcStorage.h"
#include "os/BootManager.h"
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <driver/rtc_io.h>
#include <sys/stat.h>

namespace HalInit {

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

  bool updaterMode = false;

  // Check GPIO0 (Left button) held at startup
  pinMode(0, INPUT_PULLUP);
  if (digitalRead(0) == LOW) {
    Serial.println("[Boot] LEFT button (GPIO0) held. Forcing updater mode.");
    updaterMode = true;
  }

  if (!updaterMode) {
    if (mountSdForUpdater()) {
      struct stat st;
      if (stat("/sdcard/firmware.bin", &st) == 0) {
        Serial.println(
            "[Boot] /firmware.bin found on SDMMC. Forcing updater mode.");
        updaterMode = true;
      }
    }
  }

  if (updaterMode) {
    Serial.println("[Boot] Restarting into Updater (app1)...");
    BootManager::bootUpdater();
  }
}

void prepareForSleep() {
  // Drive power rails off for sleep
  digitalWrite(2, HIGH); // Touch power off
  digitalWrite(5, HIGH); // SD power off

  // Wake up on Power button press (GPIO3, active LOW)
  rtc_gpio_init(GPIO_NUM_3);
  rtc_gpio_set_direction(GPIO_NUM_3, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_3);
  rtc_gpio_pulldown_dis(GPIO_NUM_3);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_3, 0);
}

BatteryMonitor *createBatteryMonitor() {
  BatteryMonitor::Cw2017Config cfg;
  return new BatteryMonitor(cfg);
}

IFrontlight *createFrontlight() { return new EspFrontlight(); }

} // namespace HalInit

#endif // PLATFORM_ESP32
