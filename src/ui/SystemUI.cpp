#include "SystemUI.h"
#include "../hal/IInput.h"
#include "FooterWidget.h"
#include <cstdio>

#include <GfxRenderer.h> // from Papyrix

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>

static EpdFont sysFontNormal(&ui_12);
static EpdFontFamily sysFamilyNormal(&sysFontNormal);

static EpdFont sysFontBold(&ui_bold_12);
static EpdFontFamily sysFamilyBold(&sysFontBold);
#endif

SystemUI::SystemUI(IDisplay &display) : _display(display) {}

SystemUI::~SystemUI() {}

void SystemUI::ensureFonts() {
  if (_fontsLoaded)
    return;

#ifdef PLATFORM_ESP32
  auto renderer = _display.getRenderer();
  if (renderer) {
    // ID 10 = Normal UI font, ID 11 = Bold UI font (to avoid clashing with
    // InkEngine's 0 and 1)
    renderer->insertFont(10, sysFamilyNormal);
    renderer->insertFont(11, sysFamilyBold);
  }
#endif

  _fontsLoaded = true;
}

void SystemUI::showError(const char *title, const char *message) {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (renderer) {
    _display.clear();
    int y = 50;

#ifdef PLATFORM_ESP32
    renderer->drawText(11, 50, y, title);
    y += renderer->getLineHeight(11) + 10;

    int maxWidth = _display.getWidth() - 100;
    auto wrappedLines =
        renderer->wrapTextWithHyphenation(10, message, maxWidth, 20);
    for (const auto &line : wrappedLines) {
      renderer->drawText(10, 50, y, line.c_str());
      y += renderer->getLineHeight(10);
    }
#else
    // Fallback for native
    renderer->drawText(0, 50, y, title);
    y += renderer->getLineHeight(0) + 10;

    int maxWidth = _display.getWidth() - 100;
    auto wrappedLines =
        renderer->wrapTextWithHyphenation(0, message, maxWidth, 20);
    for (const auto &line : wrappedLines) {
      renderer->drawText(0, 50, y, line.c_str());
      y += renderer->getLineHeight(0);
    }
#endif
    _display.fullRefresh();
  } else {
    printf("=== %s ===\n%s\n", title, message);
  }
}

void SystemUI::showLoading(const char *title, float progress) {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (renderer) {
    _display.clear();

#ifdef PLATFORM_ESP32
    // Draw progress bar
    int barWidth = _display.getWidth() - 100;
    int barHeight = 20;
    int barX = 50;
    int barY = (_display.getHeight() - barHeight) / 2;

    renderer->drawText(11, barX, barY - 40, title);

    // Frame
    renderer->drawRect(barX, barY, barWidth, barHeight);
    // Fill
    int fillWidth = (int)(barWidth * progress);
    if (fillWidth > 0) {
      renderer->fillRect(barX, barY, fillWidth, barHeight);
    }
#endif

    // For loading, partial refresh is better so it animates cleanly
    _display.present();
  } else {
    printf("\r%s: %d%%", title, (int)(progress * 100));
    fflush(stdout);
  }
}

void SystemUI::showSleepCover() {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (renderer) {
    _display.clear();

#ifdef PLATFORM_ESP32
    const char *splash = "E E N K";
    const char *msg = "Powered Off";

    // Placed at top-left to avoid right-edge clipping
    renderer->drawText(11, 50, 50, splash);
    renderer->drawText(10, 50, 90, msg);
#endif

    _display.fullRefresh();
  } else {
    printf("\nDevice sleeping...\n");
  }
}

bool SystemUI::showConfirmDialog(IInput &input, const char *title,
                                 const char *message) {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (!renderer)
    return false;

  const int dispW = _display.getWidth();
  const int dispH = _display.getHeight();

  static constexpr int DLG_W = 300;
  static constexpr int DLG_H = 150;
  int dlgX = (dispW - DLG_W) / 2;
  int dlgY = (dispH - DLG_H) / 2;

  int fontNormal = 10;
  int fontBold = 11;
#ifndef PLATFORM_ESP32
  fontNormal = 0;
  fontBold = 0;
#endif

  // Background + border.
  renderer->fillRect(dlgX, dlgY, DLG_W, DLG_H, false); // white fill
  renderer->drawRect(dlgX, dlgY, DLG_W, DLG_H, true);  // black border
  renderer->drawRect(dlgX + 1, dlgY + 1, DLG_W - 2, DLG_H - 2,
                     true); // 2 px border

  // Title (bold, centred).
  int tw = renderer->getTextWidth(fontBold, title);
  renderer->drawText(fontBold, dlgX + (DLG_W - tw) / 2, dlgY + 14, title, true);

  // Separator
  // renderer->drawLine(dlgX, dlgY + 40, dlgX + DLG_W, dlgY + 40, true);
  renderer->drawLine(dispW / 2, dlgY + DLG_H, dispW / 2, dispH - 48, true);

  // Message body (wrapped, max 3 lines, inner margin of 15px)
  static constexpr int marginX = 15;
  int maxTextW = DLG_W - 2 * marginX;
  auto wrappedLines =
      renderer->wrapTextWithHyphenation(fontNormal, message, maxTextW, 3);

  int msgY = dlgY + 54;
  for (const auto &line : wrappedLines) {
    renderer->drawText(fontNormal, dlgX + marginX, msgY, line.c_str(), true);
    msgY += renderer->getLineHeight(fontNormal);
  }

  // Render footer using FooterWidget
  FooterWidget footer;
  footer.btnBack = {true, "Cancel", "Back"};
  footer.btnConfirm = {false, "", ""};
  footer.btnPrev = {false, "", ""};
  footer.btnNext = {true, "Confirm", "Confirm"};
  footer.render(renderer, dispW, dispH, fontNormal);

  _display.present();

  // Wait for user response.
  while (true) {
    ButtonEvent ev = input.pollInput();
    if (ev == ButtonEvent::QUIT)
      return false;
    // if (ev == ButtonEvent::CONFIRM)
    //   return false;
    // if (ev == ButtonEvent::LEFT)
    //   return true;
    else if (ev == ButtonEvent::RIGHT)
      return true;
#ifdef PLATFORM_ESP32
    delay(16);
#endif
  }
}
