// EENK — AppSettings
// User-configurable preferences persisted in NVS namespace "settings".
// On PLATFORM_NATIVE, load() returns defaults() and save() is a no-op.
#pragma once
#include <cstdint>

struct AppSettings {
    uint8_t  storyFontIndex;    // 0..STORY_FONT_COUNT-1
    uint8_t  choiceFontIndex;   // 0..CHOICE_FONT_COUNT-1
    uint8_t  marginPx;          // pixels: 8, 16, 24, 32
    uint16_t sleepTimeoutSec;   // 0=never, 60, 120, 300
    uint8_t  inputLayoutIndex;  // which button layout
    uint8_t  refreshInterval;   // full refresh every N partials: 5, 10, 15, 20

    // ── Human-readable option lists (for UI display) ──────────────────────────

    // Story font names correspond 1:1 to the builtinFonts reader_*.h files.
    static const char* const STORY_FONT_NAMES[];
    static const int         STORY_FONT_COUNT;

    // Choice / UI font names.
    static const char* const CHOICE_FONT_NAMES[];
    static const int         CHOICE_FONT_COUNT;

    // Discrete option arrays (mirrors what the settings screen cycles through).
    static const uint8_t  MARGIN_OPTIONS[];   // {8, 16, 24, 32}
    static const uint16_t SLEEP_OPTIONS[];    // {0, 60, 120, 300}
    static const uint8_t  REFRESH_OPTIONS[];  // {5, 10, 15, 20}

    // Number of supported input layouts (button mappings).
    static constexpr int INPUT_LAYOUT_COUNT = 2;

    // ── Persistence ───────────────────────────────────────────────────────────

    // Load from NVS. Returns defaults() if NVS is empty or not available.
    static AppSettings load();

    // Save all fields to NVS.
    void save() const;

    // Factory defaults.
    static AppSettings defaults();
};
