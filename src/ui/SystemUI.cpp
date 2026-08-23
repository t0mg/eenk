#include "SystemUI.h"
#include "../hal/IInput.h"
#include "FooterWidget.h"
#include "HeaderWidget.h"
#include <cstdio>
#include <string>
#include <vector>

#include "BatteryWidget.h"
#include <GfxRenderer.h> // from Papyrix

#include "NeuStyle.h"

#ifdef PLATFORM_ESP32
#include "HalInit.h"
#include <InputManager.h>
#include <esp_sleep.h>
extern BatteryWidget *batteryWidget;
#endif

SystemUI::SystemUI(IDisplay &display) : _display(display) {}

SystemUI::~SystemUI() {}

#include "SleepCoverWidget.h"
#include "LoadingWidget.h"
#include "ModalDialogWidget.h"

void SystemUI::showMessage(const char *title, const char *message) {
  LoadingWidget::showMessage(_display, title, message);
}

void SystemUI::showLoading(const char *title, float progress) {
  LoadingWidget::show(_display, title, progress);
}

void SystemUI::showSleepCover(const char *msg, const char *title) {
  SleepCoverWidget::show(_display, msg, title);
}

bool SystemUI::showConfirmDialog(IInput &input, const char *title,
                                 const char *message, const char *headerTitle) {
#ifdef PLATFORM_ESP32
  return ModalDialogWidget::show(_display, input, batteryWidget, title, message, headerTitle);
#else
  return ModalDialogWidget::show(_display, input, nullptr, title, message, headerTitle);
#endif
}

void SystemUI::checkBatteryAndShutdown(class BatteryWidget &battery,
                                       class IDisplay &display) {
  battery.tick();
  // Shutdown if battery is critically low (e.g. <= 2%) and not charging.
  if (battery.getPercentage() <= 2 && !battery.isCharging()) {
#ifdef PLATFORM_ESP32
    SystemUI ui(display);
    ui.showSleepCover("Battery Depleted");
    // Give e-ink time to finish updating
    delay(500);
    HalInit::prepareForSleep();
    esp_deep_sleep_start();
#else
    printf("Native: Battery depleted, would power off.\n");
    exit(0);
#endif
  }
}
