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

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
// Provide a robust analogReadMilliVolts implementation on ESP32-C3 that shares the ADC
// cleanly with InputManager's analogRead() without conflicting cali handle collisions.
extern "C" uint32_t analogReadMilliVolts(uint8_t pin) {
  uint32_t raw = analogRead(pin);
  // 12-bit ADC with 12dB attenuation (nominal ~3100 mV full-scale on ESP32-C3)
  return (raw * 3100) / 4095;
}
#endif

} // namespace HalInit

#endif // PLATFORM_ESP32
