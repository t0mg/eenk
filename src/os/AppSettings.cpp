// eenk — AppSettings implementation
// NVS namespace: "settings"
//   key "sfont"     (uint8)  — storyFontIndex
//   key "cfont"     (uint8)  — choiceFontIndex
//   key "margin"    (uint8)  — marginPx
//   key "sleep"     (uint16) — sleepTimeoutSec
//   key "layout"    (uint8)  — inputLayoutIndex
//   key "refresh"   (uint8)  — refreshInterval
//   key "sfont_ovr" (uint8)  — overrideStoryFont (0/1)
//
// On PLATFORM_NATIVE all NVS calls are omitted; load() returns defaults().
#include "AppSettings.h"
#include <cstring>

// ─── Static option tables
// ─────────────────────────────────────────────────────

// Choice / UI font names.
//   0  ui_10.h        — "UI 10"
//   1  ui_bold_10.h   — "UI Bold 10"
//   2  ui_12.h        — "UI 12"        ← default
//   3  ui_bold_12.h   — "UI Bold 12"
//   4  small14.h      — "Small 14"
const char *const AppSettings::CHOICE_FONT_NAMES[] = {
    "UI 10", "UI Bold 10", "UI 12", "UI Bold 12", "Small 14",
};
const int AppSettings::CHOICE_FONT_COUNT =
    static_cast<int>(sizeof(CHOICE_FONT_NAMES) / sizeof(CHOICE_FONT_NAMES[0]));

// Discrete margin values the settings screen cycles through (px).
const uint8_t AppSettings::MARGIN_OPTIONS[] = {8, 16, 24, 32};

// Discrete sleep-timeout values (seconds; 0 = never sleep).
const uint16_t AppSettings::SLEEP_OPTIONS[] = {0, 60, 120, 300};

// Full-refresh intervals (every N partial refreshes). 0 = never.
const uint8_t AppSettings::REFRESH_OPTIONS[] = {0, 5, 10, 15, 20};

// ─── Factory defaults
// ─────────────────────────────────────────────────────────

AppSettings AppSettings::defaults() {
  AppSettings s;
  strncpy(s.storyFont, "sans-medium", sizeof(s.storyFont));
  s.storyFont[sizeof(s.storyFont) - 1] = '\0';
  s.choiceFontIndex = 2; // "UI 12"
  s.marginPx = 16;
  s.sleepTimeoutSec = 120;
  s.inputLayoutIndex = 0;
  s.refreshInterval = 10;
  s.overrideStoryFont = false;
  s.choiceCascadeMs = 350;
  s.choiceFocusDelayMs = 700;
  s.touchChoicesEnabled = true;
  s.touchScrollEnabled = true;
  s.frontLightBrightness = 50;
  s.frontLightTemperature = 50;
  return s;
}

// ─── NVS persistence ─────────────────────────────────────────────────────────

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <Preferences.h>

static constexpr char kNvsNamespace[] = "settings";
static constexpr char kKeyStoryFontStr[] = "sfont_str";
static constexpr char kKeyChoiceFont[] = "cfont";
static constexpr char kKeyMargin[] = "margin";
static constexpr char kKeySleep[] = "sleep";
static constexpr char kKeyLayout[] = "layout";
static constexpr char kKeyRefresh[] = "refresh";
static constexpr char kKeyFontOverride[] = "sfont_ovr";
static constexpr char kKeyCascadeMs[] = "casc_ms";
static constexpr char kKeyFocusMs[] = "foc_ms";
static constexpr char kKeyTouchChoices[] = "tch_chs";
static constexpr char kKeyTouchScroll[] = "tch_scr";
static constexpr char kKeyFrontLightBrightness[] = "front_led_brightness";
static constexpr char kKeyFrontLightTemperature[] = "front_led_temperature";

AppSettings AppSettings::load() {
  AppSettings s = defaults();
  Preferences prefs;

  // Open read-write so the namespace is auto-created if it doesn't exist.
  // Using read-only (true) causes NOT_FOUND on first boot.
  if (!prefs.begin(kNvsNamespace, false)) {
    return s; // NVS unavailable — return defaults
  }

  String sf = prefs.getString(kKeyStoryFontStr, s.storyFont);
  strncpy(s.storyFont, sf.c_str(), sizeof(s.storyFont) - 1);
  s.storyFont[sizeof(s.storyFont) - 1] = '\0';
  s.choiceFontIndex = prefs.getUChar(kKeyChoiceFont, s.choiceFontIndex);
  s.marginPx = prefs.getUChar(kKeyMargin, s.marginPx);
  s.sleepTimeoutSec = prefs.getUShort(kKeySleep, s.sleepTimeoutSec);
  s.inputLayoutIndex = prefs.getUChar(kKeyLayout, s.inputLayoutIndex);
  s.refreshInterval = prefs.getUChar(kKeyRefresh, s.refreshInterval);
  s.overrideStoryFont = prefs.getUChar(kKeyFontOverride, 0) != 0;
  s.choiceCascadeMs = prefs.getUShort(kKeyCascadeMs, s.choiceCascadeMs);
  s.choiceFocusDelayMs = prefs.getUShort(kKeyFocusMs, s.choiceFocusDelayMs);
  s.touchChoicesEnabled =
      prefs.getUChar(kKeyTouchChoices, s.touchChoicesEnabled ? 1 : 0) != 0;
  s.touchScrollEnabled =
      prefs.getUChar(kKeyTouchScroll, s.touchScrollEnabled ? 1 : 0) != 0;
  s.frontLightBrightness =
      prefs.getUChar(kKeyFrontLightBrightness, s.frontLightBrightness);
  s.frontLightTemperature =
      prefs.getUChar(kKeyFrontLightTemperature, s.frontLightTemperature);
  prefs.end();

  // Clamp indices to valid range (guards against corruption / version skew).
  // Font fallback is handled cleanly at runtime by SdFontCatalogue and
  // applyBuiltin No strict bound check here

  // Bounds check other things
  if (s.choiceFontIndex >= static_cast<uint8_t>(CHOICE_FONT_COUNT))
    s.choiceFontIndex = defaults().choiceFontIndex;

  return s;
}

void AppSettings::save() const {
  Preferences prefs;
  prefs.begin(kNvsNamespace, false);

  prefs.putString(kKeyStoryFontStr, storyFont);
  prefs.putUChar(kKeyChoiceFont, choiceFontIndex);
  prefs.putUChar(kKeyMargin, marginPx);
  prefs.putUShort(kKeySleep, sleepTimeoutSec);
  prefs.putUChar(kKeyLayout, inputLayoutIndex);
  prefs.putUChar(kKeyRefresh, refreshInterval);
  prefs.putUChar(kKeyFontOverride, overrideStoryFont ? 1 : 0);
  prefs.putUShort(kKeyCascadeMs, choiceCascadeMs);
  prefs.putUShort(kKeyFocusMs, choiceFocusDelayMs);
  prefs.putUChar(kKeyTouchChoices, touchChoicesEnabled ? 1 : 0);
  prefs.putUChar(kKeyTouchScroll, touchScrollEnabled ? 1 : 0);
  prefs.putUChar(kKeyFrontLightBrightness, frontLightBrightness);
  prefs.putUChar(kKeyFrontLightTemperature, frontLightTemperature);
  prefs.end();
}

// ═════════════════════════════════════════════════════════════════════════════
#else // PLATFORM_NATIVE stubs
// ═════════════════════════════════════════════════════════════════════════════

AppSettings AppSettings::load() { return defaults(); }

void AppSettings::save() const {
  // No-op on native.
}

#endif // PLATFORM_ESP32
