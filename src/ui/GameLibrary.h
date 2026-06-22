// EENK — GameLibrary
// Scrollable story browser shown in MENU boot mode.
// Scans /eenk/ on the SD card for .bin files, displays title/author/size,
// and lets the user select a story to launch or navigate to settings.
//
// run() blocks until:
//   - The user selects a story  → sets BootManager to INK_RUNTIME and reboots
//   - The user presses RIGHT or BACK → returns false (caller shows SettingsView)
#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"
#include <cstdint>
#include <cstddef>

class BatteryWidget;

class GameLibrary {
public:
    GameLibrary(IDisplay& display, IInput& input, BatteryWidget& battery, AppSettings& settings);
    ~GameLibrary() = default;

    // Run the game library loop. Blocks until the user selects a game
    // (which sets BootManager and reboots) or presses RIGHT/BACK to go to settings.
    // Returns false if user pressed RIGHT or BACK (wants settings).
    // Never returns on game launch (reboots inside).
    bool run();

private:
    IDisplay&      _display;
    IInput&        _input;
    BatteryWidget& _battery;
    AppSettings&   _settings;

    struct StoryEntry {
        char     path[128];         // "/eenk/filename.bin"
        char     title[64];         // From metadata or derived from filename
        char     author[32];        // From metadata or empty
        uint32_t sizeBytes;         // File size in bytes
        bool     hasSave;           // Has a save in /.eenk_saves/<filename>.bin.save
        bool     isCurrentlyLoaded; // Path matches NVS boot.story_path
        bool     hasMetadata;       // Whether EENK header was found
    };

    static constexpr int MAX_STORIES   = 32;
    static constexpr int STATUS_BAR_H  = 28;   // px — height of the status bar strip
    static constexpr int HINT_BAR_H    = 28;   // px — height of the bottom button hint bar
    static constexpr int ITEM_H        = 64;   // px per story list entry
    // floor((800 - STATUS_BAR_H - HINT_BAR_H) / ITEM_H)
    static constexpr int VISIBLE_ITEMS = 11;
    static constexpr int DISPLAY_W     = 480;
    static constexpr int DISPLAY_H     = 800;
    static constexpr int ITEM_MARGIN_X = 12;   // left/right text margin within an entry

    // Font IDs registered in the renderer (must not clash with InkEngine 0/1,
    // SystemUI 10/11, or BatteryWidget 20).
    static constexpr int FONT_NORMAL = 30;   // ui_12
    static constexpr int FONT_BOLD   = 31;   // ui_bold_12
    static constexpr int FONT_SMALL  = 32;   // ui_10

    StoryEntry _entries[MAX_STORIES];
    int        _numEntries    = 0;
    int        _selectedIndex = 0;
    int        _scrollOffset  = 0;
    bool       _firstRender   = true;
    bool       _fontsLoaded   = false;

    // ── Private helpers ───────────────────────────────────────────────────────

    // Register UI fonts in the renderer (idempotent).
    void ensureFonts();

    // Populate _entries[] by scanning the SD card (or local stories/ dir on native).
    void scanSD();

    // Sort _entries[0.._numEntries) alphabetically by title (insertion sort — n≤32).
    void sortEntries();

    // Derive a human-readable title from a raw filename: strip ".bin", replace
    // underscores and hyphens with spaces, capitalise first letter.
    static void titleFromFilename(const char* filename, char* outTitle, size_t outLen);

    // Format a byte count as a compact human-readable string: "3 KB", "1.2 MB".
    static void formatSize(uint32_t bytes, char* out, size_t outLen);

    // ── Rendering ─────────────────────────────────────────────────────────────

    // Full-screen redraw.
    void render();

    // Draw the top status bar (EENK title + battery widget + separator line).
    void renderStatusBar();

    // Draw one story row at vertical position yPos.
    // index: entry index in _entries[]; selected: true if currently highlighted.
    void renderEntry(int index, int yPos, bool selected);

    // Draw "No stories found" centered message.
    void renderEmpty();

    // Draw the bottom hint bar showing available button actions.
    void renderHintBar();

    // ── Actions ───────────────────────────────────────────────────────────────

    // Adjust _scrollOffset so _selectedIndex is always visible.
    void clampScroll();

    // Set BootManager state and reboot into INK_RUNTIME for entry at index.
    void launchStory(int index);
};
