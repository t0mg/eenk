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
#include <EpdFont.h>
#include <builtinFonts/syne_bold_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>

#ifdef PLATFORM_ESP32
#include <InputManager.h>
#include <esp_sleep.h>
extern BatteryWidget *batteryWidget;
#endif

static EpdFont sysFontNormal(&ui_12);
static EpdFontFamily sysFamilyNormal(&sysFontNormal);

static EpdFont sysFontBold(&ui_bold_12);
static EpdFontFamily sysFamilyBold(&sysFontBold);

static EpdFont sysFontHeading(&syne_bold_10);
static EpdFontFamily sysFamilyHeading(&sysFontHeading);

SystemUI::SystemUI(IDisplay &display) : _display(display) {}

SystemUI::~SystemUI() {}

void SystemUI::ensureFonts() {
  if (_fontsLoaded)
    return;

  auto renderer = _display.getRenderer();
  if (renderer) {
    // ID 10 = Normal UI font, ID 11 = Bold UI font (to avoid clashing with
    // InkEngine's 0 and 1)
    renderer->insertFont(10, sysFamilyNormal);
    renderer->insertFont(11, sysFamilyBold);
    renderer->insertFont(NeuStyle::FONT_HEADING, sysFamilyHeading);
    renderer->insertFont(NeuStyle::FONT_SMALL,
                         sysFamilyNormal); // reuse ui_12 as small for now
  }

  _fontsLoaded = true;
}

#include "SleepCoverWidget.h"
#include "LoadingWidget.h"
#include "ModalDialogWidget.h"

void SystemUI::showMessage(const char *title, const char *message) {
  ensureFonts();
  LoadingWidget::showMessage(_display, title, message);
}

void SystemUI::showLoading(const char *title, float progress) {
  ensureFonts();
  LoadingWidget::show(_display, title, progress);
}

void SystemUI::showSleepCover(const char *msg, const char *title) {
  ensureFonts();
  SleepCoverWidget::show(_display, msg, title);
}

bool SystemUI::showConfirmDialog(IInput &input, const char *title,
                                 const char *message) {
  ensureFonts();
#ifdef PLATFORM_ESP32
  return ModalDialogWidget::show(_display, input, batteryWidget, title, message);
#else
  return ModalDialogWidget::show(_display, input, nullptr, title, message);
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
    esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN,
                                      ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
#else
    printf("Native: Battery depleted, would power off.\n");
    exit(0);
#endif
  }
}
