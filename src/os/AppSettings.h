// eenk — AppSettings
// User-configurable preferences persisted in NVS namespace "settings".
// On PLATFORM_NATIVE, load() returns defaults() and save() is a no-op.
#pragma once
#include <cstdint>

struct AppSettings {
    char     storyFont[32];     // token (builtin) or stem (SD font), e.g. "sans-medium"
    uint8_t  choiceFontIndex;   // 0..CHOICE_FONT_COUNT-1
    uint8_t  marginPx;          // pixels: 8, 16, 24, 32
    uint16_t sleepTimeoutSec;   // 0=never, 60, 120, 300
    uint8_t  inputLayoutIndex;  // which button layout
    uint8_t  refreshInterval;   // full refresh every N partials: 5, 10, 15, 20
    bool     overrideStoryFont; // if true, storyFontIndex always wins over story @font hint

    // ── Human-readable option lists (for UI display) ──────────────────────────

    // Choice / UI font names.
    static const char* const CHOICE_FONT_NAMES[];
    static const int         CHOICE_FONT_COUNT;

    // Discrete option arrays (mirrors what the settings screen cycles through).
    static const uint8_t  MARGIN_OPTIONS[];   // {8, 16, 24, 32}
    static const uint16_t SLEEP_OPTIONS[];    // {0, 60, 120, 300}
    static const uint8_t  REFRESH_OPTIONS[];  // {5, 10, 15, 20}

    // Number of supported input layouts (button mappings).
    static constexpr int INPUT_LAYOUT_COUNT = 2;

    // NVS key for the override flag.
    static constexpr char KEY_SFONT_OVERRIDE[] = "sfont_ovr";

    // ── Persistence ───────────────────────────────────────────────────────────

    // Load from NVS. Returns defaults() if NVS is empty or not available.
    static AppSettings load();

    // Save all fields to NVS.
    void save() const;

    // Factory defaults.
    static AppSettings defaults();
};
