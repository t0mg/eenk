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
                                 IFrontlight *frontlight, AppSettings &settings,
                                 bool choicesEnabled)
    : _display(display), _input(input), _battery(battery),
      _frontlight(frontlight), _settings(settings),
      _choicesEnabled(choicesEnabled) {}

QuickMenuAction QuickMenuWidget::show() {
  render();

  int displayW = _display.getWidth();

  while (true) {
    ButtonEvent ev = _input.pollInput();

    int touchX = -1, touchY = -1;
    if (_input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0) {
      // Tap outside card (bottom half of screen) -> close
      if (touchY > 430) {
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
      } else if (touchY >= 250 && touchY < 330) { // Toggles row
        int gap = 20;
        int toggleW = (displayW - 48 - 2 * gap) / 3;
        int toggleX0 = 24;
        int toggleX1 = toggleX0 + toggleW + gap;
        int toggleX2 = toggleX1 + toggleW + gap;

        if (touchX >= toggleX0 && touchX <= toggleX0 + toggleW) {
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
        } else if (touchX >= toggleX1 && touchX <= toggleX1 + toggleW) {
          _settings.touchScrollEnabled = !_settings.touchScrollEnabled;
          _dirty = true;
        } else if (touchX >= toggleX2 && touchX <= toggleX2 + toggleW) {
          if (_choicesEnabled) {
            _settings.touchChoicesEnabled = !_settings.touchChoicesEnabled;
            _dirty = true;
          }
        }
#ifdef PLATFORM_ESP32
        delay(16);
#endif
        render();
      } else if (touchY >= 330 && touchY < 410) { // Sleep button
        if (_dirty) {
          _settings.save();
          _dirty = false;
        }
        return QuickMenuAction::SLEEP_DEVICE;
      }
    }

    switch (ev) {
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
  int cardH = 420;
  int cardX = 0;
  int cardY = 0;
  int fontHeading = NeuStyle::FONT_HEADING;

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
  int gap = 20;
  int toggleW = (cardW - 48 - 2 * gap) / 3;
  int toggleX0 = cardX + 24;
  int toggleX1 = toggleX0 + toggleW + gap;
  int toggleX2 = toggleX1 + toggleW + gap;

  // LED Toggle
  r->drawShadowBox(toggleX0, toggleY, toggleW, 60, NeuStyle::BORDER_W, 4);
  if (!_settings.frontLightEnabled) {
    r->fillRect(toggleX0 + 2, toggleY + 2, toggleW - 4, 56, true);
  }
  centerText(fontHeading, toggleX0, toggleY, toggleW, 60, "LIGHT",
             _settings.frontLightEnabled);

  // Touch Scroll Toggle
  r->drawShadowBox(toggleX1, toggleY, toggleW, 60, NeuStyle::BORDER_W, 4);
  if (!_settings.touchScrollEnabled) {
    r->fillRect(toggleX1 + 2, toggleY + 2, toggleW - 4, 56, true);
  }
  centerText(fontHeading, toggleX1, toggleY, toggleW, 60, "SWIPE",
             _settings.touchScrollEnabled);

  // Touch Choices Toggle
  r->drawShadowBox(toggleX2, toggleY, toggleW, 60, NeuStyle::BORDER_W, 4);
  if (!_choicesEnabled) {
    centerText(fontHeading, toggleX2, toggleY, toggleW, 60, "CHOICES", true);
    r->fillHalftoneRect(toggleX2 + 2, toggleY + 2, toggleW - 4, 56, true);
  } else {
    if (!_settings.touchChoicesEnabled) {
      r->fillRect(toggleX2 + 2, toggleY + 2, toggleW - 4, 56, true);
    }
    centerText(fontHeading, toggleX2, toggleY, toggleW, 60, "CHOICES",
               _settings.touchChoicesEnabled);
  }

  // Quick Action Buttons
  int btnY = cardY + 340;
  // Sleep button
  r->drawShadowBox(cardX + 24, btnY, cardW - 48, 60, NeuStyle::BORDER_W, 4);
  if (_selectedRow == 2)
    r->fillRect(cardX + 26, btnY + 2, cardW - 52, 56, true);
  centerText(fontHeading, cardX + 24, btnY, cardW - 48, 60, "SLEEP DEVICE",
             _selectedRow != 2);

  _display.present();
}
