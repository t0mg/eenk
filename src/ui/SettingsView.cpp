// eenk — SettingsView implementation (neubrutalist)
//
// Font ID allocations (must not clash with InkEngine 0/1, SystemUI 10/11,
// BatteryWidget 20):
//   NeuStyle::FONT_HEADING (12)  Syne Bold 10pt — headings and actions
//   30  ui_12       (normal body text in settings rows)
//   31  ui_bold_12  (bold labels)
//   33  ui_10       (small supplementary text)
//
// Display geometry (480 × 800 portrait):
//   Status bar:  y=0..39  (HeaderWidget::HEIGHT = 40)
//   List items:  y=40..  (ROW_H = 60 px each)
#include "SettingsView.h"
#include "BatteryWidget.h"
#include "ListItemWidget.h"
#include "NeuStyle.h"
#include "QuickMenuWidget.h"
#include "SystemUI.h"
#include "eenk_version.h"
#include "os/BootManager.h"
#include <cctype>
#include <cstring>

#include <cstdio>
#include <cstring>

#ifdef PLATFORM_ESP32
#include "HalInit.h"
#include <esp_sleep.h>
#endif

// Builtin font table (for display names in the font picker)
#include <BuiltinFonts.h>

#include <GfxRenderer.h>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <InputManager.h>
#include <esp_sleep.h>
#endif

static constexpr int kFontNormal = NeuStyle::FONT_BODY;
static constexpr int kFontBold = NeuStyle::FONT_BODY_BOLD;
static constexpr int kFontSmall = NeuStyle::FONT_SMALL;
static constexpr int kFontHeading = NeuStyle::FONT_HEADING;

static constexpr int ROW_H = NeuStyle::ROW_H;
static constexpr int LEFT_MARGIN = NeuStyle::MARGIN_X;
static constexpr int DISP_W = 480;
static constexpr int DISP_H = 800;

// Card inset from screen edges for bordered rows
static constexpr int CARD_INSET_X = 24;
static constexpr int CARD_INSET_Y = 8;
static constexpr int CARD_W = DISP_W - 2 * CARD_INSET_X;

// Helper: uppercase a string in-place
static void toUpper(char *s) {
  for (; *s; s++)
    *s = toupper((unsigned char)*s);
}

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
  if (n == 0)
    return "Off";
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
                           BatteryWidget &battery, IFrontlight *frontlight,
                           AppSettings &settings)
    : _display(display), _input(input), _battery(battery),
      _frontlight(frontlight), _settings(settings) {}

SettingsView::~SettingsView() {}

// ─── run() ───────────────────────────────────────────────────────────────────

void SettingsView::run() {

  _fontCatalogue.scan();
  _currentFontIndex = 0;
  for (size_t i = 0; i < _fontCatalogue.getCount(); ++i) {
    const FontEntry &e = _fontCatalogue.getEntries()[i];
    const char *id =
        (e.builtinIndex != 255) ? kBuiltinFonts[e.builtinIndex].token : e.stem;
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

    if (ev == ButtonEvent::TOP_EDGE_SWIPE) {
      QuickMenuWidget qm(_display, _input, _battery, _frontlight, _settings);
      QuickMenuAction act = qm.show();
      if (act == QuickMenuAction::SLEEP_DEVICE) {
        ev = ButtonEvent::SLEEP;
      } else {
        renderPage();
        continue;
      }
    }

    // TODO: this logic is broken, let's reimplement a cleaner touch handling
    // int touchX = -1, touchY = -1;
    // if (_input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >=
    // 0) {
    //   if (touchY >= 440) {
    //     if (touchX < 400) {
    //       running = false;
    //       break;
    //     } else {
    //       ev = ButtonEvent::CONFIRM;
    //     }
    //   } else {
    //     int relY = touchY - (HeaderWidget::HEIGHT + CARD_INSET_Y);
    //     if (relY >= 0) {
    //       int clickedIdx = relY / ROW_H;
    //       if (clickedIdx >= 0 && clickedIdx <= 7) {
    //         if (_itemIndex == clickedIdx) {
    //           ev = ButtonEvent::CONFIRM;
    //         } else {
    //           _itemIndex = clickedIdx;
    //           renderPage();
    //         }
    //       }
    //     }
    //   }
    // }

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
      delay(500);
      HalInit::prepareForSleep();
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

// ─── renderPage()
// ─────────────────────────────────────────────────────────────

void SettingsView::renderPage() {
  _display.clear();

  HeaderWidget header(_display, _battery);
  header.render("OPTIONS", kFontHeading);

  int y = HeaderWidget::HEIGHT + CARD_INSET_Y;
  static constexpr int kItems = static_cast<int>(SettingRow::COUNT);

  for (int r = 0; r < kItems; ++r) {
    SettingRow row = static_cast<SettingRow>(r);
    bool selected = (_itemIndex == r);
    switch (row) {
    case SettingRow::STORY_FONT: {
      const char *val = "Default";
      if (_fontCatalogue.getCount() > 0) {
        val = _fontCatalogue.getEntries()[_currentFontIndex].displayName;
      }
      drawSettingsRow(y, "Story Font", val, selected);
      break;
    }
    case SettingRow::CHOICE_FONT: {
      const char *val =
          AppSettings::CHOICE_FONT_NAMES[_settings.choiceFontIndex];
      drawSettingsRow(y, "Choice Font", val, selected);
      break;
    }
    case SettingRow::MARGIN: {
      const char *val = marginName(_settings.marginPx);
      drawSettingsRow(y, "Margin", val, selected);
      break;
    }
    case SettingRow::FULL_REFRESH: {
      const char *val = refreshName(_settings.refreshInterval);
      drawSettingsRow(y, "Full Refresh", val, selected);
      break;
    }
    case SettingRow::SLEEP_TIMEOUT: {
      const char *val = sleepName(_settings.sleepTimeoutSec);
      drawSettingsRow(y, "Sleep & Save", val, selected);
      break;
    }
    case SettingRow::OVERRIDE_FONT: {
      const char *val = _settings.overrideStoryFont ? "On" : "Off";
      drawSettingsRow(y, "Override Story Font", val, selected);
      break;
    }
#if !FREEINK_DEVICE_X4PRO
    case SettingRow::REBOOT_UPDATER: {
      drawSettingsRow(y, "Reboot to Updater", "OTA / App1", selected);
      break;
    }
#endif
    case SettingRow::FIRMWARE_VERSION: {
      drawSettingsRow(y, "Firmware", EENK_VERSION_STR, selected);
      break;
    }
    default:
      break;
    }
    y += ROW_H;
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
  footer.btnBack = {true, "Exit", "Back"};
  footer.btnConfirm = {true, "Change", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(r, DISP_W, DISP_H);
}

// ─── drawSettingsRow()
// ────────────────────────────────────────────────────────

void SettingsView::drawSettingsRow(int y, const char *label, const char *value,
                                   bool selected) {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  int cardX = CARD_INSET_X;
  int cardY = y + CARD_INSET_Y / 2;
  int cardW = CARD_W;
  int cardH = ROW_H - CARD_INSET_Y;

  ListItemWidget::draw(
      r, cardX, cardY, cardW, cardH, selected,
      [r, label, value, selected](int inX, int inY, int inW, int inH) {
        int labelH = r->getLineHeight(kFontHeading);
        int bodyH = r->getLineHeight(kFontNormal);

        // Convert label to uppercase for neubrutalist style
        char upperLabel[64];
        strncpy(upperLabel, label, sizeof(upperLabel) - 1);
        upperLabel[sizeof(upperLabel) - 1] = '\0';
        toUpper(upperLabel);

        int labelX = inX + LEFT_MARGIN;
        int labelY = inY + (inH - labelH) / 2;

        if (selected) {
          // Selected: draw label inside a rounded pill badge
          int pillY = inY + (inH - NeuStyle::PILL_H) / 2;
          r->drawPill(kFontHeading, labelX, pillY, upperLabel,
                      NeuStyle::PILL_PADDING_X, NeuStyle::PILL_H,
                      NeuStyle::PILL_RADIUS, true);
        } else {
          // Normal: plain heading text
          r->drawText(kFontHeading, labelX, labelY, upperLabel, true);
        }

        // Value right-aligned within card
        if (value && value[0] != '\0') {
          int textW = r->getTextWidth(kFontNormal, value);
          int valueX = inX + inW - LEFT_MARGIN - textW;
          int valueMinX = labelX + 120;
          if (valueX < valueMinX)
            valueX = valueMinX;
          int valueY = inY + (inH - bodyH) / 2;
          r->drawText(kFontNormal, valueX, valueY, value, true);
        }
      });
}

// ─── handleInput()
// ────────────────────────────────────────────────────────────

void SettingsView::handleInput(ButtonEvent ev) {
  static constexpr int kItems = static_cast<int>(SettingRow::COUNT);

  // ── Navigation (UP, DOWN, LEFT, RIGHT all move the selection) ─────────────
  if (ev == ButtonEvent::UP || ev == ButtonEvent::LEFT) {
    _itemIndex = (_itemIndex - 1 + kItems) % kItems;
    renderPage();
    return;
  }
  if (ev == ButtonEvent::DOWN || ev == ButtonEvent::RIGHT) {
    _itemIndex = (_itemIndex + 1) % kItems;
    renderPage();
    return;
  }

  // ── CONFIRM / SLEEP: cycle value or trigger action ─────────────────────────
  if (ev == ButtonEvent::CONFIRM || ev == ButtonEvent::SLEEP) {
    SettingRow row = static_cast<SettingRow>(_itemIndex);
    switch (row) {
#if !FREEINK_DEVICE_X4PRO
    case SettingRow::REBOOT_UPDATER:
      BootManager::bootUpdater();
      return;
#endif
    case SettingRow::FIRMWARE_VERSION:
      return; // Read-only
    case SettingRow::STORY_FONT:
      _dirty = true;
      if (_fontCatalogue.getCount() > 0) {
        _currentFontIndex = (_currentFontIndex + 1) % _fontCatalogue.getCount();
        const FontEntry &e = _fontCatalogue.getEntries()[_currentFontIndex];
        const char *id = (e.builtinIndex != 255)
                             ? kBuiltinFonts[e.builtinIndex].token
                             : e.stem;
        strncpy(_settings.storyFont, id, sizeof(_settings.storyFont));
        _settings.storyFont[sizeof(_settings.storyFont) - 1] = '\0';
      }
      break;
    case SettingRow::CHOICE_FONT:
      _dirty = true;
      _settings.choiceFontIndex = (uint8_t)((_settings.choiceFontIndex + 1) %
                                            AppSettings::CHOICE_FONT_COUNT);
      break;
    case SettingRow::MARGIN: {
      _dirty = true;
      static const uint8_t kMargins[] = {8, 16, 24, 32};
      static const int kMCount = 4;
      int cur = 1;
      for (int i = 0; i < kMCount; i++) {
        if (kMargins[i] == _settings.marginPx) {
          cur = i;
          break;
        }
      }
      _settings.marginPx = kMargins[(cur + 1) % kMCount];
      break;
    }
    case SettingRow::FULL_REFRESH: {
      _dirty = true;
      static const uint8_t kRefresh[] = {0, 5, 10, 15, 20};
      static const int kRCount = 5;
      int cur = 0;
      for (int i = 0; i < kRCount; i++) {
        if (kRefresh[i] == _settings.refreshInterval) {
          cur = i;
          break;
        }
      }
      _settings.refreshInterval = kRefresh[(cur + 1) % kRCount];
      break;
    }
    case SettingRow::SLEEP_TIMEOUT: {
      _dirty = true;
      static const uint16_t kSleep[] = {0, 60, 120, 300};
      static const int kSCount = 4;
      int cur = 0;
      for (int i = 0; i < kSCount; i++) {
        if (kSleep[i] == _settings.sleepTimeoutSec) {
          cur = i;
          break;
        }
      }
      _settings.sleepTimeoutSec = kSleep[(cur + 1) % kSCount];
      break;
    }
    case SettingRow::OVERRIDE_FONT:
      _dirty = true;
      _settings.overrideStoryFont = !_settings.overrideStoryFont;
      break;
    default:
      break;
    }
    renderPage();
    return;
  }
}
