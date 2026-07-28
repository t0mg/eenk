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

void SystemUI::showError(const char *title, const char *message) {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (renderer) {
    _display.clear();
    int y = 50;

    std::string msgStr(message);
    std::vector<std::string> paragraphs;
    size_t start = 0;
    size_t end = msgStr.find('\n');
    while (end != std::string::npos) {
      paragraphs.push_back(msgStr.substr(start, end - start));
      start = end + 1;
      end = msgStr.find('\n', start);
    }
    paragraphs.push_back(msgStr.substr(start));

    renderer->drawText(11, 50, y, title);
    y += renderer->getLineHeight(11) + 10;

    int maxWidth = _display.getWidth() - 100;
    for (const auto &para : paragraphs) {
      auto wrappedLines =
          renderer->wrapTextWithHyphenation(10, para.c_str(), maxWidth, 20);
      for (const auto &line : wrappedLines) {
        renderer->drawText(10, 50, y, line.c_str());
        y += renderer->getLineHeight(10);
      }
    }
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

void SystemUI::showSleepCover(const char *msg, const char *title) {
  ensureFonts();

  auto renderer = _display.getRenderer();
  if (renderer) {
    _display.clear();

#ifdef PLATFORM_ESP32
    if (title && title[0] != '\0') {
      renderer->drawText(11, 50, 50, title);
    } else {
      renderer->drawText(11, 50, 50, "eenk");
    }
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
  int dlgY = (dispH - DLG_H) / 2;

  // Adjust dialog Y to account for title bar overlapping the top
  if (hasTitle) {
    dlgY += titleBarH / 2;
  }

  // Draw halftone background overlay
  renderer->fillHalftoneRect(0, 0, dispW, dispH, false);

  // Shadow box: 6px border, 20px offset shadow
  renderer->drawShadowBox(dlgX, dlgY, DLG_W, DLG_H, NeuStyle::BORDER_W,
                          NeuStyle::SHADOW_OFFSET);

#ifdef PLATFORM_ESP32
  if (batteryWidget) {
    HeaderWidget header(_display, *batteryWidget);
    header.render("", fontHeading);
  }
#endif

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
