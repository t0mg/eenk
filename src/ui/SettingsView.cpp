// eenk — SettingsView implementation
//
// Font ID allocations (must not clash with InkEngine 0/1, SystemUI 10/11,
// BatteryWidget 20):
//   30  ui_12       (normal body text in settings rows)
//   31  ui_bold_12  (bold labels, status bar, page headers)
//   32  small14     (diagram labels on the Input page)
//   33  ui_10       (small supplementary text)
//
// Display geometry (480 × 800 portrait):
//   Status bar:  y=0..31  (HeaderWidget::HEIGHT = 32)
//   List items:  y=32..751 (48px per row) height: ROW_H = 60 px
#include "SettingsView.h"
#include <cstring>
#include "BatteryWidget.h"
#include "SystemUI.h"
#include "os/BootManager.h"

#include <cstdio>
#include <cstring>

// Builtin font table (for display names in the font picker)
#include <BuiltinFonts.h>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <InputManager.h>
#include <builtinFonts/small14.h>
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>
#include <esp_sleep.h>

static EpdFont s_font12(&ui_12);
static EpdFontFamily s_fam12(&s_font12);

static EpdFont s_fontBold12(&ui_bold_12);
static EpdFontFamily s_famBold12(&s_fontBold12);

static EpdFont s_fontSmall14(&small14);
static EpdFontFamily s_famSmall14(&s_fontSmall14);

static EpdFont s_font10(&ui_10);
static EpdFontFamily s_fam10(&s_font10);
#else
#include <GfxRenderer.h>
#endif

static constexpr int kFontNormal = 30;
static constexpr int kFontBold = 31;
static constexpr int kFontDiagram = 32;
static constexpr int kFontSmall = 33;

static constexpr int ROW_H = 60;
static constexpr int LEFT_MARGIN = 8;
static constexpr int DISP_W = 480;
static constexpr int DISP_H = 800;

// ─── ButtonAnnotation / InputLayout ──────────────────────────────────────────

struct ButtonAnnotation {
  const char *buttonName; // label shown near the button
  int bx, by;         // button position relative to diagram origin (top-left)
  int bw, bh;         // button size in pixels
  const char *action; // action description
  bool labelRight;    // if true, label is drawn to the right of the button
};

// Two layouts – cycle with LEFT/RIGHT on the Input page.
// Coords are relative to the diagram rectangle origin.
static const ButtonAnnotation kLayout0[] = {
    {"Power", 82, 0, 36, 12, "Sleep/Save", true},
    {"Left", 0, 140, 20, 28, "Back (Scroll Up)", true},
    {"Right", 0, 178, 20, 28, "(Scroll Down)", true},
    {"D-Pad", 40, 310, 40, 40, "Up/Prev Choice", true},
    {"D-Pad", 40, 360, 40, 40, "Down/Next Choice", true},
    {"OK", 18, 310, 20, 20, "Select", false},
};
static const int kLayout0Count = 6;

static const ButtonAnnotation kLayout1[] = {
    {"Power", 82, 0, 36, 12, "Sleep/Save", true},
    {"Left", 0, 140, 20, 28, "Page Back", true},
    {"Right", 0, 178, 20, 28, "Page Forward", true},
    {"D-Pad", 40, 310, 40, 40, "Prev Choice", true},
    {"D-Pad", 40, 360, 40, 40, "Next Choice", true},
    {"OK", 18, 310, 20, 20, "Confirm", false},
};
static const int kLayout1Count = 6;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Map marginPx value (8/16/24/32) to a display string.
static const char *marginName(uint8_t px) {
  if (px <= 8)
    return "8 px (Tight)";
  if (px <= 16)
    return "16 px (Normal)";
  if (px <= 24)
    return "24 px (Wide)";
  return "32 px (Extra Wide)";
}

// Map refreshInterval value (0/5/10/15/20) to a display string.
static const char *refreshName(uint8_t n) {
  if (n == 0) return "Off";
  static char buf[24];
  snprintf(buf, sizeof(buf), "Every %u updates", (unsigned)n);
  return buf;
}

// Map sleepTimeoutSec to a display string.
static const char *sleepName(uint16_t sec) {
  if (sec == 0)
    return "Never";
  if (sec <= 60)
    return "1 minute";
  if (sec <= 120)
    return "2 minutes";
  return "5 minutes";
}

// ─── Construction / Destruction
// ───────────────────────────────────────────────

SettingsView::SettingsView(IDisplay &display, IInput &input,
                           BatteryWidget &battery, AppSettings &settings)
    : _display(display), _input(input), _battery(battery), _settings(settings) {
}

SettingsView::~SettingsView() {}

// ─── run() ───────────────────────────────────────────────────────────────────

void SettingsView::run() {
  // Register fonts once
  auto *r = _display.getRenderer();
  if (r) {
#ifdef PLATFORM_ESP32
    r->insertFont(kFontNormal, s_fam12);
    r->insertFont(kFontBold, s_famBold12);
    r->insertFont(kFontDiagram, s_famSmall14);
    r->insertFont(kFontSmall, s_fam10);
#endif
  }

  _fontCatalogue.scan();
  _currentFontIndex = 0;
  for (size_t i = 0; i < _fontCatalogue.getCount(); ++i) {
      const FontEntry& e = _fontCatalogue.getEntries()[i];
      const char* id = (e.builtinIndex != 255) ? kBuiltinFonts[e.builtinIndex].token : e.stem;
      if (strcmp(_settings.storyFont, id) == 0) {
          _currentFontIndex = (int)i;
          break;
      }
  }

  _pageIndex = 0;
  _itemIndex = 0;
  bool running = true;

#ifdef PLATFORM_ESP32
  // Drain stale button events from the previous screen's button press.
  // Without this, a BACK press that exits GameLibrary immediately re-fires
  // in SettingsView and exits it too, making it look like a screen refresh.
  {
    unsigned long drainUntil = millis() + 150;
    while (millis() < drainUntil) {
      _input.pollInput();
      delay(10);
    }
  }
#endif

  renderPage();

  while (running) {
    SystemUI::checkBatteryAndShutdown(_battery, _display);

    ButtonEvent ev = _input.pollInput();
    if (ev == ButtonEvent::NONE) {
#ifdef PLATFORM_ESP32
      delay(16);
#endif
      continue;
    }

    // BACK / QUIT on page 0 exits the settings view.
    if ((ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) &&
        _pageIndex == 0) {
      running = false;
      break;
    }

    // BACK / QUIT on other pages goes to the previous page.
    if ((ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) &&
        _pageIndex > 0) {
      _pageIndex--;
      _itemIndex = 0;
      renderPage();
      continue;
    }

    if (ev == ButtonEvent::SLEEP) {
#ifdef PLATFORM_ESP32
      // Save settings before sleeping
      if (_dirty)
        _settings.save();

      {
        SystemUI ui(_display);
        ui.showSleepCover();
      }
      esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN,
                                        ESP_GPIO_WAKEUP_GPIO_LOW);
      esp_deep_sleep_start();
#endif
      continue;
    }

    // Dispatch to per-page handler.
    switch (_pageIndex) {
    case 0:
      handleReadingInput(ev);
      break;
    case 1:
      handleInputPageInput(ev);
      break;
    case 2:
      handleDangerInput(ev);
      break;
    default:
      break;
    }
  }

  // Persist if anything changed.
  if (_dirty) {
    _settings.save();
  }
}

// ─── renderPage()
// ─────────────────────────────────────────────────────────────

void SettingsView::renderPage() {
  _display.clear();

  switch (_pageIndex) {
  case 0:
    renderReadingPage();
    break;
  case 1:
    renderInputPage();
    break;
  case 2:
    renderDangerPage();
    break;
  default:
    break;
  }

  renderFooter();

  _display.present();
}

// ─── renderFooter()
// ───────────────────────────────────────────────────────────

void SettingsView::renderFooter() {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  FooterWidget footer;
  if (_pageIndex == 0) {
    footer.btnBack = {true, "Exit", "Back"};
    footer.btnConfirm = {false, "", ""};
    footer.btnPrev = {true, "Up", "Prev"};
    footer.btnNext = {true, "Down", "Next"};
  } else {
    footer.btnBack = {true, "Back", "Back"};
    footer.btnConfirm = {(_pageIndex == 3), "Apply", "Confirm"};
    footer.btnPrev = {true, "Up", "Prev"};
    footer.btnNext = {true, "Down", "Next"};
  }
  footer.render(r, DISP_W, DISP_H, kFontSmall);
}

// ─── drawSettingsRow()
// ────────────────────────────────────────────────────────

void SettingsView::drawSettingsRow(int y, const char *label, const char *value,
                                   bool selected) {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  if (selected) {
    r->fillRect(0, y, DISP_W, ROW_H, true);
  }
  bool ink = !selected; // selected row: white text on black

  // Label on the left.
  int textY = y + (ROW_H - 12) / 2; // vertically centre within row
  r->drawText(kFontBold, LEFT_MARGIN + 8, textY, label, ink);

  // Value right-aligned.
  if (value && value[0] != '\0') {
    int textW = r->getTextWidth(kFontNormal, value);
    int valueX = DISP_W - textW - 16;
    if (valueX < LEFT_MARGIN + 120)
      valueX = LEFT_MARGIN + 120;
    r->drawText(kFontNormal, valueX, textY, value, ink);
  }

  // Separator line at the bottom of the row (not drawn when selected so the
  // fill covers the whole cell cleanly).
  if (!selected) {
    r->drawLine(0, y + ROW_H - 1, DISP_W, y + ROW_H - 1, true);
  }
}

// ─── renderReadingPage()
// ──────────────────────────────────────────────────────

void SettingsView::renderReadingPage() {
  HeaderWidget header(_display, _battery);
  header.render("Settings — Reading", kFontBold);

  int y = HeaderWidget::HEIGHT;

  // Row 0: Story Font (cycles through SdFontCatalogue)
  {
    const char *val = "Default";
    if (_fontCatalogue.getCount() > 0) {
        val = _fontCatalogue.getEntries()[_currentFontIndex].displayName;
    }
    drawSettingsRow(y, "Story Font", val, _itemIndex == 0);
    y += ROW_H;
  }
  // Row 1: Choice Font
  {
    const char *val = AppSettings::CHOICE_FONT_NAMES[_settings.choiceFontIndex];
    drawSettingsRow(y, "Choice Font", val, _itemIndex == 1);
    y += ROW_H;
  }
  // Row 2: Margin
  {
    const char *val = marginName(_settings.marginPx);
    drawSettingsRow(y, "Margin", val, _itemIndex == 2);
    y += ROW_H;
  }
  // Row 3: Full Refresh
  {
    const char *val = refreshName(_settings.refreshInterval);
    drawSettingsRow(y, "Full Refresh", val, _itemIndex == 3);
    y += ROW_H;
  }
  // Row 4: Sleep & Save
  {
    const char *val = sleepName(_settings.sleepTimeoutSec);
    drawSettingsRow(y, "Sleep & Save", val, _itemIndex == 4);
    y += ROW_H;
  }
  // Row 5: Override Story Font
  //   When ON  → storyFontIndex always wins, even if the story has a @font hint.
  //   When OFF → the story’s @font hint takes precedence when present.
  {
    const char *val = _settings.overrideStoryFont ? "On" : "Off";
    drawSettingsRow(y, "Override Story Font", val, _itemIndex == 5);
  }
}

// (Behaviour page removed)

// ─── renderInputPage()
// ────────────────────────────────────────────────────────

void SettingsView::renderInputPage() {
  HeaderWidget header(_display, _battery);
  header.render("Settings — Input Mapping", kFontBold);

  auto *r = _display.getRenderer();
  if (!r)
    return;

  // Layout name header.
  static const char *kLayoutNames[2] = {"Layout A (Default)", "Layout B"};
  int layoutIdx = _settings.inputLayoutIndex;
  if (layoutIdx >= AppSettings::INPUT_LAYOUT_COUNT)
    layoutIdx = 0;

  char headerTxt[48];
  snprintf(headerTxt, sizeof(headerTxt), "< %s >", kLayoutNames[layoutIdx]);
  int hw = r->getTextWidth(kFontBold, headerTxt);
  r->drawText(kFontBold, (DISP_W - hw) / 2, HeaderWidget::HEIGHT + 8, headerTxt, true);

  // Device diagram.
  int diagW = 200;
  int diagX = (DISP_W - diagW) / 2;
  int diagY = HeaderWidget::HEIGHT + 40;
  int diagH = DISP_H - HeaderWidget::HEIGHT - FooterWidget::HEIGHT - 100;
  drawDeviceDiagram(diagX, diagY, diagW, diagH, layoutIdx);

  // Hint at bottom.
  const char *hint = "LEFT/RIGHT to change layout";
  int hintW = r->getTextWidth(kFontSmall, hint);
  r->drawText(kFontSmall, (DISP_W - hintW) / 2, DISP_H - 30, hint, true);
}

// ─── drawDeviceDiagram() ─────────────────────────────────────────────────────

void SettingsView::drawDeviceDiagram(int x, int y, int width, int height,
                                     int layoutIndex) {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  // ── Device body (rounded via thick border approximation) ─────────────────
  r->drawRect(x, y, width, height, true);
  r->drawRect(x + 1, y + 1, width - 2, height - 2, true); // 2px border

  // Screen bezel inset (10 px each side, 20 top, 80 bottom).
  int bezX = x + 10;
  int bezY = y + 20;
  int bezW = width - 20;
  int bezH = height - 100;
  r->drawRect(bezX, bezY, bezW, bezH, true);

  // ── Physical buttons ──────────────────────────────────────────────────────
  // Power button: top-centre strip.
  int pwrW = 36, pwrH = 10;
  int pwrX = x + (width - pwrW) / 2;
  int pwrY = y - pwrH;
  r->fillRect(pwrX, pwrY, pwrW, pwrH, true);

  // Left side: two thumb buttons.
  int sideW = 16, sideH = 26;
  int sideX = x - sideW;
  r->fillRect(sideX, y + 130, sideW, sideH, true);
  r->fillRect(sideX, y + 170, sideW, sideH, true);

  // D-pad (cross shape at bottom centre).
  int dpadCX = x + width / 2;
  int dpadCY = y + height - 60;
  int dpadArmW = 14, dpadArmH = 12, dpadCoreH = 12;
  // Up arm
  r->fillRect(dpadCX - dpadArmW / 2, dpadCY - dpadCoreH - dpadArmH, dpadArmW,
              dpadArmH, true);
  // Down arm
  r->fillRect(dpadCX - dpadArmW / 2, dpadCY + dpadCoreH, dpadArmW, dpadArmH,
              true);
  // Left arm
  r->fillRect(dpadCX - dpadCoreH - dpadArmH, dpadCY - dpadArmW / 2, dpadArmH,
              dpadArmW, true);
  // Right arm
  r->fillRect(dpadCX + dpadCoreH, dpadCY - dpadArmW / 2, dpadArmH, dpadArmW,
              true);
  // Centre
  r->fillRect(dpadCX - dpadCoreH, dpadCY - dpadCoreH, dpadCoreH * 2,
              dpadCoreH * 2, true);

  // OK / Confirm button (bottom-left of D-pad cluster).
  int okR = 8;
  r->fillRect(dpadCX - 40 - okR, dpadCY - okR, okR * 2, okR * 2, true);

  // ── Annotation lines + labels ─────────────────────────────────────────────
  const ButtonAnnotation *layout = kLayout0;
  int layoutCount = kLayout0Count;
  if (layoutIndex == 1) {
    layout = kLayout1;
    layoutCount = kLayout1Count;
  }

  // Button screen-space origin centres — map from ButtonAnnotation relative
  // coords to absolute positions using diagram origin x, y.
  struct ButtonPos {
    const char *buttonName;
    int cx, cy; // centre x, y
  };

  // Pre-computed button centres (absolute).

  // Line endpoints for annotation labels.
  for (int i = 0; i < layoutCount; i++) {
    const ButtonAnnotation &ann = layout[i];
    int bCX = x + ann.bx + ann.bw / 2;
    int bCY = y + ann.by + ann.bh / 2;

    // Override with computed centres for hardware buttons.
    if (strcmp(ann.buttonName, "Power") == 0) {
      bCX = pwrX + pwrW / 2;
      bCY = pwrY + pwrH / 2;
    } else if (strcmp(ann.buttonName, "Left") == 0) {
      bCX = sideX + sideW / 2;
      bCY = y + 130 + sideH / 2;
    } else if (strcmp(ann.buttonName, "Right") == 0) {
      bCX = sideX + sideW / 2;
      bCY = y + 170 + sideH / 2;
    } else if (strcmp(ann.buttonName, "OK") == 0) {
      bCX = dpadCX - 40;
      bCY = dpadCY;
    }

    // Label position.
    int lineLen = 40;
    int labelX, labelY;
    int lineEndX, lineEndY;
    if (ann.labelRight) {
      lineEndX = bCX + lineLen;
      lineEndY = bCY;
      labelX = lineEndX + 4;
      labelY = lineEndY - 6;
    } else {
      lineEndX = bCX - lineLen;
      lineEndY = bCY;
      int tw = r->getTextWidth(kFontDiagram, ann.action);
      labelX = lineEndX - tw - 4;
      labelY = lineEndY - 6;
    }

    // Clamp label to display bounds.
    if (labelX < 2)
      labelX = 2;
    if (labelX + 100 > DISP_W)
      labelX = DISP_W - 104;

    r->drawLine(bCX, bCY, lineEndX, lineEndY, true);
    r->drawText(kFontDiagram, labelX, labelY, ann.action, true);
  }
}

// ─── renderDangerPage()
// ───────────────────────────────────────────────────────

void SettingsView::renderDangerPage() {
  HeaderWidget header(_display, _battery);
  header.render("Settings — System", kFontBold);

  auto *r = _display.getRenderer();
  if (!r)
    return;

  int y = HeaderWidget::HEIGHT;

  // Inverted title header.
  r->fillRect(0, y, DISP_W, ROW_H, true);
  const char *dangerTitle = "! System !";
  int tw = r->getTextWidth(kFontBold, dangerTitle);
  r->drawText(kFontBold, (DISP_W - tw) / 2, y + (ROW_H - 12) / 2, dangerTitle,
              false);
  y += ROW_H;

  drawSettingsRow(y, "Update Firmware", "Requires /wifi.txt on SD", _itemIndex == 0);
  y += ROW_H;
  drawSettingsRow(y, "Delete Save", "Current story save file", _itemIndex == 1);
  y += ROW_H;
  drawSettingsRow(y, "Delete Story", "Remove .bin from SD card",
                  _itemIndex == 2);
  y += ROW_H;
  drawSettingsRow(y, "Format SD", "Erase entire SD card!", _itemIndex == 3);
}

// ─── handleReadingInput() ────────────────────────────────────────────────────

void SettingsView::handleReadingInput(ButtonEvent ev) {
  static constexpr int kItems = 6; // 0=StoryFont, 1=ChoiceFont, 2=Margin, 3=Refresh, 4=Sleep, 5=Override

  switch (ev) {
  case ButtonEvent::UP:
    _itemIndex = (_itemIndex - 1 + kItems) % kItems;
    renderPage();
    break;

  case ButtonEvent::DOWN:
    _itemIndex = (_itemIndex + 1) % kItems;
    renderPage();
    break;

  case ButtonEvent::LEFT: {
    _dirty = true;
    switch (_itemIndex) {
    case 0:
      if (_fontCatalogue.getCount() > 0) {
          _currentFontIndex = (_currentFontIndex - 1 + _fontCatalogue.getCount()) % _fontCatalogue.getCount();
          const FontEntry& e = _fontCatalogue.getEntries()[_currentFontIndex];
          const char* id = (e.builtinIndex != 255) ? kBuiltinFonts[e.builtinIndex].token : e.stem;
          strncpy(_settings.storyFont, id, sizeof(_settings.storyFont));
          _settings.storyFont[sizeof(_settings.storyFont)-1] = '\0';
      }
      break;
    case 1:
      _settings.choiceFontIndex = (uint8_t)((_settings.choiceFontIndex - 1 +
                                             AppSettings::CHOICE_FONT_COUNT) %
                                            AppSettings::CHOICE_FONT_COUNT);
      break;
    case 2: {
      static const uint8_t kMargins[] = {8, 16, 24, 32};
      static const int kMCount = 4;
      int cur = 1;
      for (int i = 0; i < kMCount; i++) {
        if (kMargins[i] == _settings.marginPx) {
          cur = i;
          break;
        }
      }
      cur = (cur - 1 + kMCount) % kMCount;
      _settings.marginPx = kMargins[cur];
      break;
    }
    case 3: {
      static const uint8_t kRefresh[] = {0, 5, 10, 15, 20};
      static const int kRCount = 5;
      int cur = 0;
      for (int i = 0; i < kRCount; i++) {
        if (kRefresh[i] == _settings.refreshInterval) {
          cur = i;
          break;
        }
      }
      cur = (cur - 1 + kRCount) % kRCount;
      _settings.refreshInterval = kRefresh[cur];
      break;
    }
    case 4: {
      static const uint16_t kSleep[] = {0, 60, 120, 300};
      static const int kSCount = 4;
      int cur = 0;
      for (int i = 0; i < kSCount; i++) {
        if (kSleep[i] == _settings.sleepTimeoutSec) {
          cur = i;
          break;
        }
      }
      cur = (cur - 1 + kSCount) % kSCount;
      _settings.sleepTimeoutSec = kSleep[cur];
      break;
    }
    case 5:
      _settings.overrideStoryFont = false;
      break;
    default:
      break;
    }
    renderPage();
    break;
  }

  case ButtonEvent::RIGHT: {
    _dirty = true;
    switch (_itemIndex) {
    case 0:
      if (_fontCatalogue.getCount() > 0) {
          _currentFontIndex = (_currentFontIndex + 1) % _fontCatalogue.getCount();
          const FontEntry& e = _fontCatalogue.getEntries()[_currentFontIndex];
          const char* id = (e.builtinIndex != 255) ? kBuiltinFonts[e.builtinIndex].token : e.stem;
          strncpy(_settings.storyFont, id, sizeof(_settings.storyFont));
          _settings.storyFont[sizeof(_settings.storyFont)-1] = '\0';
      }
      break;
    case 1:
      _settings.choiceFontIndex = (uint8_t)((_settings.choiceFontIndex + 1) %
                                            AppSettings::CHOICE_FONT_COUNT);
      break;
    case 2: {
      static const uint8_t kMargins[] = {8, 16, 24, 32};
      static const int kMCount = 4;
      int cur = 1;
      for (int i = 0; i < kMCount; i++) {
        if (kMargins[i] == _settings.marginPx) {
          cur = i;
          break;
        }
      }
      cur = (cur + 1) % kMCount;
      _settings.marginPx = kMargins[cur];
      break;
    }
    case 3: {
      static const uint8_t kRefresh[] = {0, 5, 10, 15, 20};
      static const int kRCount = 5;
      int cur = 0;
      for (int i = 0; i < kRCount; i++) {
        if (kRefresh[i] == _settings.refreshInterval) {
          cur = i;
          break;
        }
      }
      cur = (cur + 1) % kRCount;
      _settings.refreshInterval = kRefresh[cur];
      break;
    }
    case 4: {
      static const uint16_t kSleep[] = {0, 60, 120, 300};
      static const int kSCount = 4;
      int cur = 0;
      for (int i = 0; i < kSCount; i++) {
        if (kSleep[i] == _settings.sleepTimeoutSec) {
          cur = i;
          break;
        }
      }
      cur = (cur + 1) % kSCount;
      _settings.sleepTimeoutSec = kSleep[cur];
      break;
    }
    case 5:
      _settings.overrideStoryFont = true;
      break;
    default:
      break;
    }
    renderPage();
    break;
  }

  case ButtonEvent::CONFIRM:
    // Toggle the override setting with CONFIRM when on that row.
    if (_itemIndex == 5) {
      _settings.overrideStoryFont = !_settings.overrideStoryFont;
      _dirty = true;
      renderPage();
    } else if (_pageIndex < PAGE_COUNT - 1) {
      // Navigate forward to next page on confirm (when not on override row).
      _pageIndex++;
      _itemIndex = 0;
      renderPage();
    }
    break;

  default:
    break;
  }
}



// ─── handleInputPageInput() ──────────────────────────────────────────────────

void SettingsView::handleInputPageInput(ButtonEvent ev) {
  switch (ev) {
  case ButtonEvent::LEFT:
    _settings.inputLayoutIndex = (uint8_t)((_settings.inputLayoutIndex - 1 +
                                            AppSettings::INPUT_LAYOUT_COUNT) %
                                           AppSettings::INPUT_LAYOUT_COUNT);
    _dirty = true;
    renderPage();
    break;

  case ButtonEvent::RIGHT:
    _settings.inputLayoutIndex = (uint8_t)((_settings.inputLayoutIndex + 1) %
                                           AppSettings::INPUT_LAYOUT_COUNT);
    _dirty = true;
    renderPage();
    break;

  case ButtonEvent::CONFIRM:
    if (_pageIndex < PAGE_COUNT - 1) {
      _pageIndex++;
      _itemIndex = 0;
      renderPage();
    }
    break;

  default:
    break;
  }
}

// ─── handleDangerInput() ─────────────────────────────────────────────────────

void SettingsView::handleDangerInput(ButtonEvent ev) {
  static constexpr int kItems = 4;

  switch (ev) {
  case ButtonEvent::UP:
    _itemIndex = (_itemIndex - 1 + kItems) % kItems;
    renderPage();
    break;

  case ButtonEvent::DOWN:
    _itemIndex = (_itemIndex + 1) % kItems;
    renderPage();
    break;

  case ButtonEvent::CONFIRM:
    switch (_itemIndex) {
    case 0:
      BootManager::bootUpdater();
      break;
    case 1:
      deleteSave();
      break;
    case 2:
      deleteStory();
      break;
    case 3:
      formatSD();
      break;
    default:
      break;
    }
    break;

  default:
    break;
  }
}

// ─── Danger zone actions
// ──────────────────────────────────────────────────────

void SettingsView::deleteSave() {
  SystemUI ui(_display);
  if (!ui.showConfirmDialog(_input, "Delete Save?",
                            "This will erase your current progress.")) {
    renderPage();
    return;
  }

#ifdef PLATFORM_ESP32
  // Delete the save file from SD.  The save path is written by main.cpp into
  // NVS — for now we remove the well-known save directory.
  // A proper implementation would retrieve the path from BootManager / NVS.
  // (Full implementation pending GameLibrary integration.)
  Serial.println("[Settings] Delete save requested (not yet wired to path).");
#endif

  renderPage();
}

void SettingsView::deleteStory() {
  SystemUI ui(_display);
  if (!ui.showConfirmDialog(_input, "Delete Story?",
                            "This removes the .bin file from the SD card.")) {
    renderPage();
    return;
  }

#ifdef PLATFORM_ESP32
  Serial.println("[Settings] Delete story requested (not yet wired to path).");
#endif

  renderPage();
}

void SettingsView::formatSD() {
  SystemUI ui(_display);
  // Two-step confirm for the most destructive action.
  if (!ui.showConfirmDialog(_input, "Format SD?",
                            "ALL data will be erased. Are you sure?")) {
    renderPage();
    return;
  }
  if (!ui.showConfirmDialog(
          _input, "FINAL WARNING",
          "Press CONFIRM to erase everything on the SD card.")) {
    renderPage();
    return;
  }

#ifdef PLATFORM_ESP32
  // TODO: Unmount and format the SD card via ESP-IDF f_mkfs.
  Serial.println("[Settings] Format SD requested (not yet implemented).");
#endif

  renderPage();
}
