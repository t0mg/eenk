#include "QuickMenuWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>
#include <cstdio>
#include <algorithm>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <builtinFonts/syne_bold_10.h>
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>

static EpdFont s_qmNormal(&ui_12);
static EpdFontFamily s_qmFamilyNormal(&s_qmNormal);
static EpdFont s_qmBold(&ui_bold_12);
static EpdFontFamily s_qmFamilyBold(&s_qmBold);
static EpdFont s_qmHeading(&syne_bold_10);
static EpdFontFamily s_qmFamilyHeading(&s_qmHeading);
#endif

QuickMenuWidget::QuickMenuWidget(IDisplay &display, IInput &input,
                                 BatteryWidget &battery, IFrontlight *frontlight,
                                 AppSettings &settings)
    : _display(display), _input(input), _battery(battery),
      _frontlight(frontlight), _settings(settings) {}

QuickMenuAction QuickMenuWidget::show() {
  render();

  while (true) {
    ButtonEvent ev = _input.pollInput();

    int touchX = -1, touchY = -1;
    if (_input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0) {
      // Tap outside card (bottom half of screen) -> close
      if (touchY > 360) {
        return QuickMenuAction::CLOSE;
      }

      // Tap on Sliders / Action items inside card (y: 60..340)
      if (touchY >= 80 && touchY < 140) { // Cool brightness slider
        _selectedRow = 0;
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX - 220) * 100 / 480));
          _frontlight->setBrightness(pct);
        }
        render();
      } else if (touchY >= 140 && touchY < 200) { // Warmth slider
        _selectedRow = 1;
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX - 220) * 100 / 480));
          _frontlight->setWarmth(pct);
        }
        render();
      } else if (touchY >= 220 && touchY < 280) { // Action row
        if (touchX < 260) {
          return QuickMenuAction::SLEEP_DEVICE;
        } else if (touchX < 520) {
          return QuickMenuAction::OPEN_SETTINGS;
        } else {
          return QuickMenuAction::CLOSE;
        }
      }
    }

    switch (ev) {
    case ButtonEvent::UP:
      if (_selectedRow > 0) {
        _selectedRow--;
        render();
      }
      break;
    case ButtonEvent::DOWN:
      if (_selectedRow < 4) {
        _selectedRow++;
        render();
      }
      break;
    case ButtonEvent::LEFT:
      if (_selectedRow == 0 && _frontlight) {
        int b = std::max(0, (int)_frontlight->getBrightness() - 10);
        _frontlight->setBrightness(b);
        render();
      } else if (_selectedRow == 1 && _frontlight) {
        int w = std::max(0, (int)_frontlight->getWarmth() - 10);
        _frontlight->setWarmth(w);
        render();
      }
      break;
    case ButtonEvent::RIGHT:
      if (_selectedRow == 0 && _frontlight) {
        int b = std::min(100, (int)_frontlight->getBrightness() + 10);
        _frontlight->setBrightness(b);
        render();
      } else if (_selectedRow == 1 && _frontlight) {
        int w = std::min(100, (int)_frontlight->getWarmth() + 10);
        _frontlight->setWarmth(w);
        render();
      }
      break;
    case ButtonEvent::CONFIRM:
      if (_selectedRow == 2) return QuickMenuAction::SLEEP_DEVICE;
      if (_selectedRow == 3) return QuickMenuAction::OPEN_SETTINGS;
      if (_selectedRow == 4) return QuickMenuAction::CLOSE;
      break;
    case ButtonEvent::BACK:
    case ButtonEvent::QUIT:
    case ButtonEvent::SWIPE_UP:
      return QuickMenuAction::CLOSE;
    default:
#ifdef PLATFORM_ESP32
      delay(16);
#endif
      break;
    }
  }
}

void QuickMenuWidget::render() {
  _display.clear();

  GfxRenderer *r = _display.getRenderer();
  if (!r) return;

#ifdef PLATFORM_ESP32
  r->insertFont(0, s_qmFamilyNormal);
  r->insertFont(1, s_qmFamilyBold);
  r->insertFont(2, s_qmFamilyHeading);
#endif

  int cardW = 760;
  int cardH = 320;
  int cardX = (800 - cardW) / 2;
  int cardY = 20;

  // Draw card background container
  r->drawRect(cardX, cardY, cardW, cardH, true);
  r->fillRect(cardX + 2, cardY + 2, cardW - 4, cardH - 4, false);

  // Header inside card
  char headerBuf[64];
  uint16_t battPct = _battery.getPercentage();
  bool isCharging = _battery.isCharging();
  snprintf(headerBuf, sizeof(headerBuf), "QUICK SETTINGS — BATTERY %u%%%s",
           battPct, isCharging ? " (CHARGING)" : "");
  r->drawText(cardX + 24, cardY + 16, 0, headerBuf, false);

  // Divider line
  r->drawLine(cardX + 16, cardY + 50, cardX + cardW - 16, cardY + 50, true);

  // Row 0: Brightness slider
  uint8_t coolB = _frontlight ? _frontlight->getBrightness() : 0;
  char bBuf[32];
  snprintf(bBuf, sizeof(bBuf), "Cool Light: %u%%", coolB);
  r->drawText(cardX + 24, cardY + 70, 0, bBuf, false,
              _selectedRow == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  r->drawRect(cardX + 240, cardY + 72, 480, 16, true);
  int coolW = (476 * coolB) / 100;
  if (coolW > 0) {
    r->fillRect(cardX + 242, cardY + 74, coolW, 12, true);
  }

  // Row 1: Warmth slider
  uint8_t warmB = _frontlight ? _frontlight->getWarmth() : 0;
  char wBuf[32];
  snprintf(wBuf, sizeof(wBuf), "Warm Light: %u%%", warmB);
  r->drawText(cardX + 24, cardY + 130, 0, wBuf, false,
              _selectedRow == 1 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  r->drawRect(cardX + 240, cardY + 132, 480, 16, true);
  int warmW = (476 * warmB) / 100;
  if (warmW > 0) {
    r->fillRect(cardX + 242, cardY + 134, warmW, 12, true);
  }

  // Divider line
  r->drawLine(cardX + 16, cardY + 190, cardX + cardW - 16, cardY + 190, true);

  // Quick Action Buttons
  int btnY = cardY + 220;
  // Sleep button
  r->drawRect(cardX + 24, btnY, 220, 50, true);
  if (_selectedRow == 2) r->fillRect(cardX + 26, btnY + 2, 216, 46, true);
  r->drawText(cardX + 80, btnY + 16, 0, "SLEEP", _selectedRow == 2);

  // Settings button
  r->drawRect(cardX + 270, btnY, 220, 50, true);
  if (_selectedRow == 3) r->fillRect(cardX + 272, btnY + 2, 216, 46, true);
  r->drawText(cardX + 315, btnY + 16, 0, "SETTINGS", _selectedRow == 3);

  // Close button
  r->drawRect(cardX + 516, btnY, 220, 50, true);
  if (_selectedRow == 4) r->fillRect(cardX + 518, btnY + 2, 216, 46, true);
  r->drawText(cardX + 575, btnY + 16, 0, "CLOSE", _selectedRow == 4);

  _display.present();
}
