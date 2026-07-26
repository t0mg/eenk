// eenk — SettingsView implementation
//
// Font ID allocations (must not clash with InkEngine 0/1, SystemUI 10/11,
// BatteryWidget 20):
//   30  ui_12       (normal body text in settings rows)
//   31  ui_bold_12  (bold labels, status bar, page headers)
//   32  small14     (diagram labels)
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

// ─── Construction / Destruction ──────────────────────────────────────────────

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

  _itemIndex = 0;
  bool running = true;

#ifdef PLATFORM_ESP32
  // Drain stale button events from the previous screen's button press.
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

    if (ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) {
      running = false;
      break;
    }

    if (ev == ButtonEvent::SLEEP) {
#ifdef PLATFORM_ESP32
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

    handleInput(ev);
  }

  // Persist if anything changed.
  if (_dirty) {
    _settings.save();
  }
}

// ─── renderPage() ─────────────────────────────────────────────────────────────

void SettingsView::renderPage() {
  _display.clear();

  HeaderWidget header(_display, _battery);
  header.render("Settings", kFontBold);

  int y = HeaderWidget::HEIGHT;

  // Row 0: Story Font
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
  {
    const char *val = _settings.overrideStoryFont ? "On" : "Off";
    drawSettingsRow(y, "Override Story Font", val, _itemIndex == 5);
    y += ROW_H;
  }
  // Row 6: Reboot into Updater
  {
    drawSettingsRow(y, "Reboot to Updater", "OTA / App1", _itemIndex == 6);
    y += ROW_H;
  }
  // Row 7: Format SD
  {
    drawSettingsRow(y, "Format SD", "Erase SD Card", _itemIndex == 7);
  }

  renderFooter();

  _display.present();
}

// ─── renderFooter() ───────────────────────────────────────────────────────────

void SettingsView::renderFooter() {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  FooterWidget footer;
  footer.btnBack = {true, "Exit", "Back"};
  bool isActionRow = (_itemIndex == 5 || _itemIndex == 6 || _itemIndex == 7);
  footer.btnConfirm = {isActionRow, isActionRow ? "Select" : "", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};

  footer.render(r, DISP_W, DISP_H, kFontSmall);
}

// ─── drawSettingsRow() ────────────────────────────────────────────────────────

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

  // Separator line at the bottom of the row
  if (!selected) {
    r->drawLine(0, y + ROW_H - 1, DISP_W, y + ROW_H - 1, true);
  }
}

// ─── handleInput() ────────────────────────────────────────────────────────────

void SettingsView::handleInput(ButtonEvent ev) {
  static constexpr int kItems = 8;

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
      _settings.overrideStoryFont = !_settings.overrideStoryFont;
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
      _settings.overrideStoryFont = !_settings.overrideStoryFont;
      break;
    default:
      break;
    }
    renderPage();
    break;
  }

  case ButtonEvent::CONFIRM:
    if (_itemIndex == 5) {
      _settings.overrideStoryFont = !_settings.overrideStoryFont;
      _dirty = true;
      renderPage();
    } else if (_itemIndex == 6) {
      BootManager::bootUpdater();
    } else if (_itemIndex == 7) {
      formatSD();
    }
    break;

  default:
    break;
  }
}

// ─── Danger zone actions ──────────────────────────────────────────────────────

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
  Serial.println("[Settings] Format SD requested.");
#endif

  renderPage();
}
