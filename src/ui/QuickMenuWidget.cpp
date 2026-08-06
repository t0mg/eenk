#include "QuickMenuWidget.h"
#include "HeaderWidget.h"
#include "LoadingWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>
#include <algorithm>
#include <cstdio>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#endif

QuickMenuWidget::QuickMenuWidget(IDisplay &display, IInput &input,
                                 BatteryWidget &battery,
                                 IFrontlight *frontlight, AppSettings &settings)
    : _display(display), _input(input), _battery(battery),
      _frontlight(frontlight), _settings(settings) {}

QuickMenuAction QuickMenuWidget::show() {
  render();

  int displayW = _display.getWidth();

  while (true) {
    ButtonEvent ev = _input.pollInput();

    int touchX = -1, touchY = -1;
    if (_input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0) {
      // Tap outside card (bottom half of screen) -> close
      if (touchY > 360) {
        return QuickMenuAction::CLOSE;
      }

      // Tap on Sliders / Action items inside card (y: 60..340)
      if (touchY >= 70 && touchY < 150) { // Brightness slider
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX * 100 / displayW)));
          _frontlight->setBrightness(pct);
#ifdef PLATFORM_ESP32
          delay(16);
#endif
          render();
        }
      } else if (touchY >= 160 && touchY < 240) { // Warmth slider
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX * 100 / displayW)));
          _frontlight->setColorTemperature(pct);
#ifdef PLATFORM_ESP32
          delay(16);
#endif
          render();
        }
        // } else if (touchY >= 220 && touchY < 280) { // Action row
        //   if (touchX < 260) {
        //     return QuickMenuAction::SLEEP_DEVICE;
        //   } else if (touchX < 520) {
        //     return QuickMenuAction::OPEN_SETTINGS;
        //   } else {
        //     return QuickMenuAction::CLOSE;
        //   }
      }
    }

    switch (ev) {
    // case ButtonEvent::UP:
    //   if (_selectedRow > 0) {
    //     _selectedRow--;
    //     render();
    //   }
    //   break;
    // case ButtonEvent::DOWN:
    //   if (_selectedRow < 4) {
    //     _selectedRow++;
    //     render();
    //   }
    //   break;
    // case ButtonEvent::LEFT:
    //   if (_selectedRow == 0 && _frontlight) {
    //     int b = std::max(0, (int)_frontlight->getBrightness() - 10);
    //     _frontlight->setBrightness(b);
    //     render();
    //   } else if (_selectedRow == 1 && _frontlight) {
    //     int w = std::max(0, (int)_frontlight->getColorTemperature() - 10);
    //     _frontlight->setColorTemperature(w);
    //     render();
    //   }
    //   break;
    // case ButtonEvent::RIGHT:
    //   if (_selectedRow == 0 && _frontlight) {
    //     int b = std::min(100, (int)_frontlight->getBrightness() + 10);
    //     _frontlight->setBrightness(b);
    //     render();
    //   } else if (_selectedRow == 1 && _frontlight) {
    //     int w = std::min(100, (int)_frontlight->getColorTemperature() + 10);
    //     _frontlight->setColorTemperature(w);
    //     render();
    //   }
    //   break;
    // case ButtonEvent::CONFIRM:
    //   if (_selectedRow == 2)
    //     return QuickMenuAction::SLEEP_DEVICE;
    //   if (_selectedRow == 3)
    //     return QuickMenuAction::OPEN_SETTINGS;
    //   if (_selectedRow == 4)
    //     return QuickMenuAction::CLOSE;
    //   break;
    case ButtonEvent::BACK:
    case ButtonEvent::QUIT:
    case ButtonEvent::SWIPE_UP:
      return QuickMenuAction::CLOSE;
    default:
      break;
    }
#ifdef PLATFORM_ESP32
    delay(16);
#endif
  }
}

void QuickMenuWidget::render() {
  GfxRenderer *r = _display.getRenderer();
  if (!r)
    return;

  int cardW = _display.getWidth();
  int cardH = 320;
  int cardX = 0;
  int cardY = 0;
  int fontHeading = NeuStyle::FONT_HEADING;
  int fontBody = NeuStyle::FONT_BODY;

  // Draw halftone background overlay
  r->fillHalftoneRect(0, 0, _display.getWidth(), _display.getHeight(), false);

  // Draw card container
  r->drawShadowBox(cardX, cardY, cardW, cardH, NeuStyle::BORDER_W,
                   NeuStyle::SHADOW_OFFSET);

  // Header
  HeaderWidget header(_display, _battery);
  header.render("QUICK SETTINGS", fontHeading);

  // Sliders
  uint8_t brightnessPct = _frontlight ? _frontlight->getBrightness() : 0;
  LoadingWidget::show(_display, cardX + NeuStyle::MARGIN_X, cardY + 70,
                      cardW - 2 * NeuStyle::MARGIN_X, 80, "Brightness",
                      brightnessPct / 100.0f, false);

  uint8_t warmthPct = _frontlight ? _frontlight->getColorTemperature() : 0;
  LoadingWidget::show(_display, cardX + NeuStyle::MARGIN_X,
                      cardY + 70 + 80 + 10, cardW - 2 * NeuStyle::MARGIN_X, 80,
                      "Warmth", warmthPct / 100.0f, false);

  // // Divider line
  // r->drawLine(cardX + 16, cardY + 190, cardX + cardW - 16, cardY + 190,
  // true);

  // Quick Action Buttons
  // int btnY = cardY + 220;
  // // Sleep button
  // r->drawRect(cardX + 24, btnY, 220, 50, true);
  // if (_selectedRow == 2)
  //   r->fillRect(cardX + 26, btnY + 2, 216, 46, true);
  // r->drawText(fontBody, cardX + 80, btnY + 16, "SLEEP", _selectedRow == 2);

  // // Settings button
  // r->drawRect(cardX + 270, btnY, 220, 50, true);
  // if (_selectedRow == 3)
  //   r->fillRect(cardX + 272, btnY + 2, 216, 46, true);
  // r->drawText(fontBody, cardX + 315, btnY + 16, "SETTINGS", true);

  // // Close button
  // r->drawRect(cardX + 516, btnY, 220, 50, true);
  // if (_selectedRow == 4)
  //   r->fillRect(cardX + 518, btnY + 2, 216, 46, true);
  // r->drawText(fontBody, cardX + 575, btnY + 16, "CLOSE", true);

  _display.present();
}
