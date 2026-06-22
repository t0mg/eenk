// EENK — AppSettings implementation
// NVS namespace: "settings"
//   key "sfont"   (uint8)  — storyFontIndex
//   key "cfont"   (uint8)  — choiceFontIndex
//   key "margin"  (uint8)  — marginPx
//   key "sleep"   (uint16) — sleepTimeoutSec
//   key "layout"  (uint8)  — inputLayoutIndex
//   key "refresh" (uint8)  — refreshInterval
//
// On PLATFORM_NATIVE all NVS calls are omitted; load() returns defaults().
#include "AppSettings.h"

// ─── Static option tables ─────────────────────────────────────────────────────

// Story font names (index matches storyFontIndex).  These are shown verbatim
// in the settings UI.  The order mirrors the builtinFonts headers:
//   0  reader_xsmall_regular_2b.h  — "Reader XSmall"
//   1  reader_xsmall_italic_2b.h   — "Reader XSmall Italic"
//   2  reader_2b.h                 — "Reader Regular"
//   3  reader_italic_2b.h          — "Reader Italic"
//   4  reader_bold_2b.h            — "Reader Bold"
//   5  reader_medium_2b.h          — "Reader Medium"       ← default
//   6  reader_medium_italic_2b.h   — "Reader Medium Italic"
//   7  reader_medium_bold_2b.h     — "Reader Medium Bold"
//   8  reader_large_2b.h           — "Reader Large"
//   9  reader_large_italic_2b.h    — "Reader Large Italic"
//  10  reader_large_bold_2b.h      — "Reader Large Bold"
const char* const AppSettings::STORY_FONT_NAMES[] = {
    "Reader XSmall",
    "Reader XSmall Italic",
    "Reader Regular",
    "Reader Italic",
    "Reader Bold",
    "Reader Medium",
    "Reader Medium Italic",
    "Reader Medium Bold",
    "Reader Large",
    "Reader Large Italic",
    "Reader Large Bold",
};
const int AppSettings::STORY_FONT_COUNT =
    static_cast<int>(sizeof(STORY_FONT_NAMES) / sizeof(STORY_FONT_NAMES[0]));

// Choice / UI font names.
//   0  ui_10.h        — "UI 10"
//   1  ui_bold_10.h   — "UI Bold 10"
//   2  ui_12.h        — "UI 12"        ← default
//   3  ui_bold_12.h   — "UI Bold 12"
//   4  small14.h      — "Small 14"
const char* const AppSettings::CHOICE_FONT_NAMES[] = {
    "UI 10",
    "UI Bold 10",
    "UI 12",
    "UI Bold 12",
    "Small 14",
};
const int AppSettings::CHOICE_FONT_COUNT =
    static_cast<int>(sizeof(CHOICE_FONT_NAMES) / sizeof(CHOICE_FONT_NAMES[0]));

// Discrete margin values the settings screen cycles through (px).
const uint8_t AppSettings::MARGIN_OPTIONS[] = {8, 16, 24, 32};

// Discrete sleep-timeout values (seconds; 0 = never sleep).
const uint16_t AppSettings::SLEEP_OPTIONS[] = {0, 60, 120, 300};

// Full-refresh intervals (every N partial refreshes).
const uint8_t AppSettings::REFRESH_OPTIONS[] = {5, 10, 15, 20};

// ─── Factory defaults ─────────────────────────────────────────────────────────

AppSettings AppSettings::defaults() {
    AppSettings s;
    s.storyFontIndex   = 5;    // "Reader Medium"
    s.choiceFontIndex  = 2;    // "UI 12"
    s.marginPx         = 16;
    s.sleepTimeoutSec  = 120;
    s.inputLayoutIndex = 0;
    s.refreshInterval  = 10;
    return s;
}

// ─── NVS persistence ─────────────────────────────────────────────────────────

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <Preferences.h>

static constexpr char kNvsNamespace[] = "settings";
static constexpr char kKeyStoryFont[] = "sfont";
static constexpr char kKeyChoiceFont[]= "cfont";
static constexpr char kKeyMargin[]    = "margin";
static constexpr char kKeySleep[]     = "sleep";
static constexpr char kKeyLayout[]    = "layout";
static constexpr char kKeyRefresh[]   = "refresh";

AppSettings AppSettings::load() {
    AppSettings s = defaults();
    Preferences prefs;
    
    // Open read-write so the namespace is auto-created if it doesn't exist.
    // Using read-only (true) causes NOT_FOUND on first boot.
    if (!prefs.begin(kNvsNamespace, false)) {
        return s;  // NVS unavailable — return defaults
    }

    s.storyFontIndex   = prefs.getUChar(kKeyStoryFont, s.storyFontIndex);
    s.choiceFontIndex  = prefs.getUChar(kKeyChoiceFont, s.choiceFontIndex);
    s.marginPx         = prefs.getUChar(kKeyMargin, s.marginPx);
    s.sleepTimeoutSec  = prefs.getUShort(kKeySleep, s.sleepTimeoutSec);
    s.inputLayoutIndex = prefs.getUChar(kKeyLayout, s.inputLayoutIndex);
    s.refreshInterval  = prefs.getUChar(kKeyRefresh, s.refreshInterval);

    prefs.end();

    // Clamp indices to valid range (guards against corruption / version skew).
    if (s.storyFontIndex  >= static_cast<uint8_t>(STORY_FONT_COUNT))
        s.storyFontIndex  = defaults().storyFontIndex;
    if (s.choiceFontIndex >= static_cast<uint8_t>(CHOICE_FONT_COUNT))
        s.choiceFontIndex = defaults().choiceFontIndex;

    return s;
}

void AppSettings::save() const {
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    
    prefs.putUChar(kKeyStoryFont,  storyFontIndex);
    prefs.putUChar(kKeyChoiceFont, choiceFontIndex);
    prefs.putUChar(kKeyMargin,     marginPx);
    prefs.putUShort(kKeySleep,     sleepTimeoutSec);
    prefs.putUChar(kKeyLayout,     inputLayoutIndex);
    prefs.putUChar(kKeyRefresh,    refreshInterval);

    prefs.end();
}

// ═════════════════════════════════════════════════════════════════════════════
#else  // PLATFORM_NATIVE stubs
// ═════════════════════════════════════════════════════════════════════════════

AppSettings AppSettings::load() {
    return defaults();
}

void AppSettings::save() const {
    // No-op on native.
}

#endif  // PLATFORM_ESP32
