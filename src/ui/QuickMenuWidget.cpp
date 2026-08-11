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
      if (touchY > 420) {
        if (_dirty) {
          _settings.save();
          _dirty = false;
        }
        return QuickMenuAction::CLOSE;
      }

      // Tap on Sliders / Action items inside card (y: 60..420)
      if (touchY >= 70 && touchY < 150) { // Brightness slider
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX * 100 / displayW)));
          _frontlight->setBrightness(pct);
          _settings.frontLightBrightness = pct;
          _settings.frontLightEnabled = true;
          _dirty = true;
#ifdef PLATFORM_ESP32
          delay(16);
#endif
          render();
        }
      } else if (touchY >= 160 && touchY < 240) { // Warmth slider
        if (_frontlight) {
          int pct = std::max(0, std::min(100, (touchX * 100 / displayW)));
          _frontlight->setColorTemperature(pct);
          _settings.frontLightTemperature = pct;
          _settings.frontLightEnabled = true;
          _dirty = true;
#ifdef PLATFORM_ESP32
          delay(16);
#endif
          render();
        }
      } else if (touchY >= 250 && touchY < 320) { // Toggles row
        if (touchX >= 24 && touchX <= 150) {
          _settings.frontLightEnabled = !_settings.frontLightEnabled;
          if (_frontlight) {
            if (_settings.frontLightEnabled) {
              _frontlight->setBrightness(_settings.frontLightBrightness);
              _frontlight->setColorTemperature(_settings.frontLightTemperature);
            } else {
              _frontlight->setBrightness(0);
            }
          }
          _dirty = true;
        } else if (touchX >= 170 && touchX <= 296) {
          _settings.touchScrollEnabled = !_settings.touchScrollEnabled;
          _dirty = true;
        } else if (touchX >= 316 && touchX <= 442) {
          _settings.touchChoicesEnabled = !_settings.touchChoicesEnabled;
          _dirty = true;
        }
#ifdef PLATFORM_ESP32
        delay(16);
#endif
        render();
      } else if (touchY >= 330 && touchY < 400) { // Sleep button
        if (_dirty) {
          _settings.save();
          _dirty = false;
        }
        return QuickMenuAction::SLEEP_DEVICE;
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
      if (_dirty) {
        _settings.save();
        _dirty = false;
      }
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
  int cardH = 410;
  int cardX = 0;
  int cardY = 0;
  int fontHeading = NeuStyle::FONT_HEADING;
  int fontBody = NeuStyle::FONT_BODY;
  int fontSmall = NeuStyle::FONT_SMALL;

  // Draw halftone background overlay
  r->fillHalftoneRect(0, 0, _display.getWidth(), _display.getHeight(), false);

  // Draw card container
  r->drawShadowBox(cardX, cardY, cardW, cardH, NeuStyle::BORDER_W,
                   NeuStyle::SHADOW_OFFSET);

  // Header
  HeaderWidget header(_display, _battery);
  header.render("QUICK SETTINGS", fontHeading);

  // Sliders
  uint8_t brightnessPct = _settings.frontLightBrightness;
  LoadingWidget::show(_display, cardX + NeuStyle::MARGIN_X, cardY + 70,
                      cardW - 2 * NeuStyle::MARGIN_X, 80, "Brightness",
                      brightnessPct / 100.0f, false);

  uint8_t warmthPct = _settings.frontLightTemperature;
  LoadingWidget::show(_display, cardX + NeuStyle::MARGIN_X,
                      cardY + 70 + 80 + 10, cardW - 2 * NeuStyle::MARGIN_X, 80,
                      "Warmth", warmthPct / 100.0f, false);

  // Divider line
  r->fillRect(cardX + 16, cardY + 246, cardW - 32, NeuStyle::BORDER_W, true);

  // Helper for centering text in a box
  auto centerText = [&](int font, int boxX, int boxY, int boxW, int boxH,
                        const char *text, bool black) {
    int tw = r->getTextWidth(font, text);
    int th = r->getLineHeight(font);
    r->drawText(font, boxX + (boxW - tw) / 2, boxY + (boxH - th) / 2, text,
                black);
  };

  // Toggles Row
  int toggleY = cardY + 260;

  // LED Toggle
  r->drawShadowBox(cardX + 24, toggleY, 126, 60, NeuStyle::BORDER_W, 4);
  if (_settings.frontLightEnabled) {
    r->fillRect(cardX + 26, toggleY + 2, 122, 56, true);
  }
  centerText(fontBody, cardX + 24, toggleY, 126, 60, "LIGHT",
             !_settings.frontLightEnabled);

  // Touch Scroll Toggle
  r->drawShadowBox(cardX + 170, toggleY, 126, 60, NeuStyle::BORDER_W, 4);
  if (_settings.touchScrollEnabled) {
    r->fillRect(cardX + 172, toggleY + 2, 122, 56, true);
  }
  centerText(fontBody, cardX + 170, toggleY, 126, 60, "SWIPE",
             !_settings.touchScrollEnabled);

  // Touch Choices Toggle
  r->drawShadowBox(cardX + 316, toggleY, 126, 60, NeuStyle::BORDER_W, 4);
  if (_settings.touchChoicesEnabled) {
    r->fillRect(cardX + 318, toggleY + 2, 122, 56, true);
  }
  centerText(fontBody, cardX + 316, toggleY, 126, 60, "CHOICES",
             !_settings.touchChoicesEnabled);

  // Quick Action Buttons
  int btnY = cardY + 340;
  // Sleep button
  r->drawShadowBox(cardX + 24, btnY, cardW - 48, 50, NeuStyle::BORDER_W, 4);
  if (_selectedRow == 2)
    r->fillRect(cardX + 26, btnY + 2, cardW - 52, 46, true);
  centerText(fontBody, cardX + 24, btnY, cardW - 48, 50, "SLEEP DEVICE",
             _selectedRow != 2);

  _display.present();
}
