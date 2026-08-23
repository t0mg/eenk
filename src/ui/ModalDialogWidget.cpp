#include "ModalDialogWidget.h"
#include "FooterWidget.h"
#include "HeaderWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>
#include <cstdio>
#include <string>
#include <vector>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#endif

bool ModalDialogWidget::show(IDisplay &display, IInput &input,
                             BatteryWidget *batteryWidget, const char *title,
                             const char *message, const char *headerTitle) {
  auto renderer = display.getRenderer();
  if (!renderer)
    return false;

  const int dispW = display.getWidth();
  const int dispH = display.getHeight();

  static constexpr int DLG_W = NeuStyle::DIALOG_W;
  int fontHeading = NeuStyle::FONT_HEADING;
  int fontBody = NeuStyle::FONT_BODY;

  static constexpr int marginX = 20;
  int maxTextW = DLG_W - 2 * marginX;

  bool hasTitle = (title && title[0] != '\0');
  int headingLineH = hasTitle ? renderer->getLineHeight(fontHeading) : 0;
  int msgLineH = renderer->getLineHeight(fontBody);

  // Auto-wrap message text
  std::vector<std::string> wrappedLines;
  if (message && message[0] != '\0') {
    std::string msgStr(message);
    size_t start = 0;
    size_t end = msgStr.find('\n');
    while (end != std::string::npos) {
      std::string para = msgStr.substr(start, end - start);
      if (!para.empty()) {
        auto lines = renderer->wrapTextWithHyphenation(fontBody, para.c_str(),
                                                       maxTextW, 20);
        wrappedLines.insert(wrappedLines.end(), lines.begin(), lines.end());
      } else {
        wrappedLines.push_back("");
      }
      start = end + 1;
      end = msgStr.find('\n', start);
    }
    std::string para = msgStr.substr(start);
    if (!para.empty()) {
      auto lines = renderer->wrapTextWithHyphenation(fontBody, para.c_str(),
                                                     maxTextW, 20);
      wrappedLines.insert(wrappedLines.end(), lines.begin(), lines.end());
    }
  }

  // Title bar height (black rect with white text)
  int titleBarH = hasTitle ? (headingLineH + 32) : 0;

  static constexpr int paddingTop = 20;
  static constexpr int titleGap = 14;
  static constexpr int paddingBottom = 20;

  int numMsgLines = static_cast<int>(wrappedLines.size());
  int msgTotalH = numMsgLines * msgLineH;
  // Content starts after the title bar overlap
  int contentH = (hasTitle ? titleBarH + titleGap - paddingTop : 0) + msgTotalH;
  int DLG_H = paddingTop + contentH + paddingBottom;

  int dlgX = (dispW - DLG_W) / 2;
  int dlgY =
      (dispH - FooterWidget::HEIGHT - NeuStyle::SHADOW_OFFSET - DLG_H) / 2;

  // Adjust dialog Y to account for title bar overlapping the top
  if (hasTitle) {
    dlgY += titleBarH / 2;
  }

  // Draw halftone background overlay
  renderer->fillHalftoneRect(0, 0, dispW, dispH, false);

  // Shadow box: 6px border, 20px offset shadow
  renderer->drawShadowBox(dlgX, dlgY, DLG_W, DLG_H, NeuStyle::BORDER_W,
                          NeuStyle::SHADOW_OFFSET);

  if (batteryWidget) {
    HeaderWidget header(display, *batteryWidget);
    header.render(headerTitle ? headerTitle : "", fontHeading);
  }

  // Title bar: solid black rectangle overlapping the dialog top border
  int currY = dlgY + paddingTop;
  if (hasTitle) {
    int titleBarY = dlgY;
    // Black rectangle spanning the dialog width, centered
    renderer->fillRect(dlgX, titleBarY, DLG_W, titleBarH, true);
    // White text centered in the black bar
    // Convert title to uppercase
    std::string upperTitle(title);
    for (auto &c : upperTitle)
      c = toupper(c);
    int tw = renderer->getTextWidth(fontHeading, upperTitle.c_str());
    int titleTextY = titleBarY + (titleBarH - headingLineH) / 2;
    renderer->drawText(fontHeading, dlgX + (DLG_W - tw) / 2, titleTextY,
                       upperTitle.c_str(), false);
    currY = dlgY + titleBarH + titleGap;
  }

  // Vertical separator line below dialog down to footer
  int lineH = (dispH - NeuStyle::FOOTER_H) - (dlgY + DLG_H);
  if (lineH > 0) {
    const int sep_W = NeuStyle::SHADOW_OFFSET + NeuStyle::BORDER_W;
    renderer->fillRect(dispW / 2 - sep_W / 2, dlgY + DLG_H, sep_W, lineH, true);
  }

  // Message body (wrapped, left-aligned)
  for (const auto &line : wrappedLines) {
    if (!line.empty()) {
      int lineX = dlgX + marginX;
      renderer->drawText(fontBody, lineX, currY, line.c_str(), true);
    }
    currY += msgLineH;
  }

  // Render footer using FooterWidget
  FooterWidget footer;
  footer.btnBack = {true, "CANCEL", "Back", false};
  footer.btnConfirm = {false, "", "", false};
  footer.btnPrev = {false, "", "", false};
  footer.btnNext = {true, "CONFIRM", "Confirm", true}; // isPill = true
  footer.render(renderer, dispW, dispH);

  display.present();

  // Wait for user response.
  while (true) {
    ButtonEvent ev = input.pollInput();
    int touchX = -1, touchY = -1;
    if (input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0) {
      input.resetActivityTimer();
      ButtonEvent touchEv = footer.getButtonEventAt(touchX, touchY, dispW, dispH);
      if (touchEv == ButtonEvent::BACK) {
        return false;
      } else if (touchEv == ButtonEvent::RIGHT || touchEv == ButtonEvent::CONFIRM) {
        return true;
      }
    }

    if (ev == ButtonEvent::QUIT || ev == ButtonEvent::LEFT || ev == ButtonEvent::BACK) {
      input.resetActivityTimer();
      return false;
    } else if (ev == ButtonEvent::RIGHT || ev == ButtonEvent::CONFIRM) {
      input.resetActivityTimer();
      return true;
    }
#ifdef PLATFORM_ESP32
    delay(16);
#endif
  }
}
