// eenk — Library
// Scrollable content browser shown in MENU boot mode.
// Scans /stories/, /books/, and root on the SD card for .bin and .epub files,
// displays title/author/size, and lets the user select content to launch or
// navigate to settings.
//
// run() blocks until:
//   - The user selects an item  → sets BootManager and reboots
//   - The user presses RIGHT or BACK → returns false (caller shows
//   SettingsView)
#pragma once
#include "FooterWidget.h"
#include "HeaderWidget.h"
#include "NeuStyle.h"
#include "hal/IDisplay.h"
#include "hal/IFrontlight.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"
#include <cstddef>
#include <cstdint>
#include <functional>


class BatteryWidget;

class Library {
public:
  Library(IDisplay &display, IInput &input, BatteryWidget &battery,
          IFrontlight *frontlight, AppSettings &settings);
  virtual ~Library() = default;

  // Run the story library loop. Blocks until the user selects a game
  // (which sets BootManager and reboots) or presses RIGHT/BACK to go to
  // settings. Returns false if user pressed RIGHT or BACK (wants settings).
  // Never returns on game launch (reboots inside).
  bool run();

  void setNeedsFullRefresh(bool needs) { _needsFullRefresh = needs; }
  void setBackgroundTask(std::function<bool()> task) { _backgroundTask = task; }

protected:
  IDisplay &_display;
  IInput &_input;
  BatteryWidget &_battery;
  IFrontlight *_frontlight;
  AppSettings &_settings;

  struct StoryEntry {
    char path[128];         // "/eenk/filename.bin" or "/books/book.epub"
    char title[64];         // From metadata or derived from filename
    char author[32];        // From metadata or empty
    uint32_t sizeBytes;     // File size in bytes
    bool hasSave;           // Has a save in /.eenk_saves/<stem>.sav
    bool isCurrentlyLoaded; // Path matches NVS boot.story_path
    bool hasMetadata;       // Whether eenk header was found
    enum class ContentType : uint8_t {
      INK_STORY = 0,
      EPUB_BOOK = 1,
    } contentType;
    uint32_t thumbOffset;   // Optional thumbnail offset in .media sidecar
    uint32_t thumbSize;     // Optional thumbnail size
    uint16_t thumbW;
    uint16_t thumbH;
    char thumbPath[128];    // EPUB cached thumbnail path
  };

  static constexpr int MAX_STORIES = 32;
  static constexpr int ITEM_H = 172; // px per story list entry
  // floor((800 - STATUS_BAR_H - FooterWidget::HEIGHT) / ITEM_H)
  static constexpr int VISIBLE_ITEMS = 4;
  static constexpr int DISPLAY_W = 480;
  static constexpr int DISPLAY_H = 800;
  static constexpr int ITEM_MARGIN_X =
      12; // left/right text margin within an entry

  void parseThumbMetadata(StoryEntry &e);
  void parseEpubThumbCache(StoryEntry &e);
  // SystemUI 10/11, or BatteryWidget 20).
  static constexpr int FONT_NORMAL = NeuStyle::FONT_BODY;      // 10
  static constexpr int FONT_BOLD   = NeuStyle::FONT_BODY_BOLD; // 11
  static constexpr int FONT_SMALL  = NeuStyle::FONT_SMALL;     // 13

  StoryEntry _entries[MAX_STORIES];
  int _numEntries = 0;
  int _selectedIndex = 0;
  int _scrollOffset = 0;
  bool _firstRender = true;
  bool _needsFullRefresh = true;
  std::function<bool()> _backgroundTask = nullptr;

  // ── Private helpers ───────────────────────────────────────────────────────

  // Populate _entries[] by scanning the SD card (or local stories/ dir on
  // native).
  virtual void scanSD();

  // Focus the currently loaded story (if any) in the list and scroll it into
  // view.
  void focusLoadedStory();

  // Sort _entries[0.._numEntries) alphabetically by title (insertion sort —
  // n≤32).
  void sortEntries();

  // Derive a human-readable title from a raw filename: strip ".bin", replace
  // underscores and hyphens with spaces, capitalise first letter.
  static void titleFromFilename(const char *filename, char *outTitle,
                                size_t outLen);

  // Derive title from full path: if filename is generic (main.bin), use parent
  // directory name.
  static void titleFromPath(const char *storyPath, char *outTitle,
                            size_t outLen);

  // Format a byte count as a compact human-readable string: "3 KB", "1.2 MB".
  static void formatSize(uint32_t bytes, char *out, size_t outLen);

  // ── Rendering ─────────────────────────────────────────────────────────────

  // Full-screen redraw.
  void render();

  // Draw one story row at vertical position yPos.
  // index: entry index in _entries[]; selected: true if currently highlighted.
  void renderEntry(int index, int yPos, bool selected);

  // Draw "No stories found" centered message.
  void renderEmpty();

  // Draw the bottom hint bar showing available button actions.
  void renderFooter();

  // ── Actions ───────────────────────────────────────────────────────────────

  // Adjust _scrollOffset so _selectedIndex is always visible.
  void clampScroll();

  // Set BootManager state and reboot into INK_RUNTIME for entry at index.
  void launchStory(int index);
};
