// EENK — GameLibrary implementation
// Scrollable story browser: scans /eenk/ on SD, shows title/author/size,
// launches selected story via BootManager.
#include "GameLibrary.h"
#include "BatteryWidget.h"
#include "StoryMetadata.h"
#include "SystemUI.h"
#include "os/BootManager.h"

#include <GfxRenderer.h>
#include <cstdio>
#include <cstring>
#include <cctype>

#ifdef PLATFORM_ESP32
#  include <Arduino.h>
#  include <SD.h>
#  include <SPI.h>
#  include <esp_sleep.h>
#  include <EpdFont.h>
#  include <EpdFontFamily.h>
#  include <builtinFonts/ui_12.h>
#  include <builtinFonts/ui_bold_12.h>
#  include <builtinFonts/ui_10.h>
#  include <InputManager.h>

static EpdFont       s_glNormal(&ui_12);
static EpdFontFamily s_glFamilyNormal(&s_glNormal);

static EpdFont       s_glBold(&ui_bold_12);
static EpdFontFamily s_glFamilyBold(&s_glBold);

static EpdFont       s_glSmall(&ui_10);
static EpdFontFamily s_glFamilySmall(&s_glSmall);

#else
// ── Native / SDL simulation ────────────────────────────────────────────────
#  include <dirent.h>
#  include <sys/stat.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────

GameLibrary::GameLibrary(IDisplay& display, IInput& input,
                         BatteryWidget& battery, AppSettings& settings)
    : _display(display), _input(input), _battery(battery), _settings(settings)
{}

// ─── ensureFonts() ────────────────────────────────────────────────────────────

void GameLibrary::ensureFonts() {
    if (_fontsLoaded) return;
#ifdef PLATFORM_ESP32
    auto* r = _display.getRenderer();
    if (r) {
        r->insertFont(FONT_NORMAL, s_glFamilyNormal);
        r->insertFont(FONT_BOLD,   s_glFamilyBold);
        r->insertFont(FONT_SMALL,  s_glFamilySmall);
    }
#endif
    _fontsLoaded = true;
}

// ─── titleFromFilename() ──────────────────────────────────────────────────────

void GameLibrary::titleFromFilename(const char* filename, char* outTitle, size_t outLen) {
    if (outLen == 0) return;

    // Copy filename, stripping ".bin" suffix (case-insensitive).
    size_t len = strlen(filename);
    if (len > 4) {
        const char* ext = filename + len - 4;
        if (ext[0] == '.' &&
            (ext[1] == 'b' || ext[1] == 'B') &&
            (ext[2] == 'i' || ext[2] == 'I') &&
            (ext[3] == 'n' || ext[3] == 'N')) {
            len -= 4;
        }
    }
    size_t copyLen = (len < outLen - 1) ? len : outLen - 1;
    strncpy(outTitle, filename, copyLen);
    outTitle[copyLen] = '\0';

    // Replace '_' and '-' with spaces; capitalise first letter.
    for (size_t i = 0; i < copyLen; i++) {
        if (outTitle[i] == '_' || outTitle[i] == '-') {
            outTitle[i] = ' ';
        }
    }
    if (copyLen > 0) {
        outTitle[0] = (char)toupper((unsigned char)outTitle[0]);
    }
}

// ─── formatSize() ─────────────────────────────────────────────────────────────

void GameLibrary::formatSize(uint32_t bytes, char* out, size_t outLen) {
    if (bytes >= 1024u * 1024u) {
        // Show as X.X MB
        uint32_t mb10 = (bytes * 10u) / (1024u * 1024u);
        if (mb10 % 10 == 0) {
            snprintf(out, outLen, "%u MB", mb10 / 10);
        } else {
            snprintf(out, outLen, "%u.%u MB", mb10 / 10, mb10 % 10);
        }
    } else {
        snprintf(out, outLen, "%u KB", (bytes + 512u) / 1024u);
    }
}

// ─── sortEntries() ────────────────────────────────────────────────────────────

void GameLibrary::sortEntries() {
    // Insertion sort — n ≤ MAX_STORIES (32), so this is fine.
    for (int i = 1; i < _numEntries; i++) {
        StoryEntry key = _entries[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(_entries[j].title, key.title) > 0) {
            _entries[j + 1] = _entries[j];
            j--;
        }
        _entries[j + 1] = key;
    }
}

// ─── scanSD() ─────────────────────────────────────────────────────────────────

void GameLibrary::scanSD() {
    _numEntries = 0;
    memset(_entries, 0, sizeof(_entries));

    // Retrieve the currently-loaded story path from NVS so we can mark it.
    char currentPath[128] = {};
    BootManager::getStoryPath(currentPath, sizeof(currentPath));

#ifdef PLATFORM_ESP32
    // ── ESP32: read from SD card ──────────────────────────────────────────────
    File dir = SD.open("/eenk");
    if (!dir || !dir.isDirectory()) return;

    // Feed the watchdog — SD.open + metadata reads can block for a while.
    yield();
    File f = dir.openNextFile();
    while (f && _numEntries < MAX_STORIES) {
        if (!f.isDirectory()) {
            String name = f.name();
            if (name.endsWith(".bin") || name.endsWith(".BIN")) {
                StoryEntry& e = _entries[_numEntries];

                // Build full path: "/eenk/<name>"
                snprintf(e.path, sizeof(e.path), "/eenk/%s", name.c_str());
                e.sizeBytes = (uint32_t)f.size();

                // Try to read StoryMetadata header.
                StoryMetadata meta;
                if (StoryMetadata::readFromSD(e.path, &meta)) {
                    e.hasMetadata = true;
                    strncpy(e.title,  meta.title,  sizeof(e.title)  - 1);
                    strncpy(e.author, meta.author, sizeof(e.author) - 1);
                    e.title[sizeof(e.title)   - 1] = '\0';
                    e.author[sizeof(e.author) - 1] = '\0';
                    // If the metadata title is empty, fall back to filename.
                    if (e.title[0] == '\0') {
                        titleFromFilename(name.c_str(), e.title, sizeof(e.title));
                    }
                } else {
                    e.hasMetadata = false;
                    titleFromFilename(name.c_str(), e.title, sizeof(e.title));
                }

                // Check for a save file at /.eenk_saves/<name>.save
                char savePath[160];
                snprintf(savePath, sizeof(savePath), "/.eenk_saves/%s.save", name.c_str());
                e.hasSave = SD.exists(savePath);

                // Mark if this is the currently-active story.
                e.isCurrentlyLoaded = (currentPath[0] != '\0' &&
                                       strcmp(e.path, currentPath) == 0);

                _numEntries++;
            }
        }
        f = dir.openNextFile();
    }
    dir.close();

#else
    // ── Native: scan local stories/ directory ────────────────────────────────
    DIR* dp = opendir("stories");
    if (!dp) return;

    struct dirent* ent;
    while ((ent = readdir(dp)) != nullptr && _numEntries < MAX_STORIES) {
        const char* name = ent->d_name;
        size_t nlen = strlen(name);
        if (nlen < 5) continue;
        const char* ext = name + nlen - 4;
        bool isBin = (ext[0] == '.' &&
                      (ext[1] == 'b' || ext[1] == 'B') &&
                      (ext[2] == 'i' || ext[2] == 'I') &&
                      (ext[3] == 'n' || ext[3] == 'N'));
        if (!isBin) continue;

        StoryEntry& e = _entries[_numEntries];
        snprintf(e.path, sizeof(e.path), "stories/%s", name);

        // File size via stat
        struct stat st;
        if (stat(e.path, &st) == 0) {
            e.sizeBytes = (uint32_t)st.st_size;
        }

        // StoryMetadata not available on native (readFromSD is a stub).
        e.hasMetadata = false;
        titleFromFilename(name, e.title, sizeof(e.title));
        e.author[0]         = '\0';
        e.hasSave           = false;
        e.isCurrentlyLoaded = false;

        _numEntries++;
    }
    closedir(dp);
#endif

    sortEntries();
}

// ─── clampScroll() ────────────────────────────────────────────────────────────

void GameLibrary::clampScroll() {
    // Ensure selected is within [0, _numEntries).
    if (_selectedIndex < 0) _selectedIndex = 0;
    if (_selectedIndex >= _numEntries) _selectedIndex = _numEntries > 0 ? _numEntries - 1 : 0;

    // Scroll up if selected is above the visible window.
    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    }
    // Scroll down if selected is below the visible window.
    if (_selectedIndex >= _scrollOffset + VISIBLE_ITEMS) {
        _scrollOffset = _selectedIndex - VISIBLE_ITEMS + 1;
    }
    // Keep scrollOffset non-negative.
    if (_scrollOffset < 0) _scrollOffset = 0;
}

// ─── renderStatusBar() ────────────────────────────────────────────────────────

void GameLibrary::renderStatusBar() {
    auto* r = _display.getRenderer();
    if (!r) return;

#ifdef PLATFORM_ESP32
    // Poll battery so the widget shows a fresh reading on first render.
    _battery.tick();

    // Clear status bar strip.
    r->fillRect(0, 0, DISPLAY_W, STATUS_BAR_H, false /*white*/);

    // Left: "EENK" in bold, vertically centred.
    int boldH  = r->getLineHeight(FONT_BOLD);
    int textY  = (STATUS_BAR_H - boldH) / 2;
    r->drawText(FONT_BOLD, ITEM_MARGIN_X, textY, "EENK", true /*black*/);

    // Right: battery widget.
    // Label is now drawn LEFT of the body, so icon body+nub (27px) sits in the
    // top-right corner. The label extends leftward automatically.
    static constexpr int kIconW = 27;  // body(24) + nub(3)
    static constexpr int kBatH  = 14;
    int batX = DISPLAY_W - kIconW - ITEM_MARGIN_X;
    int batY = (STATUS_BAR_H - kBatH) / 2;
    _battery.draw(batX, batY, false /*not inverted*/);

    // Bottom separator line.
    r->drawLine(0, STATUS_BAR_H - 1, DISPLAY_W, STATUS_BAR_H - 1, true);
#endif
}

// ─── renderEntry() ────────────────────────────────────────────────────────────

void GameLibrary::renderEntry(int index, int yPos, bool selected) {
    auto* r = _display.getRenderer();
    if (!r) return;

#ifdef PLATFORM_ESP32
    const StoryEntry& e = _entries[index];

    // Background: fill row black if selected, white otherwise.
    r->fillRect(0, yPos, DISPLAY_W, ITEM_H, selected /*ink = true means black fill*/);

    // Text colour: white on black (selected) or black on white (normal).
    bool ink = !selected;

    // Top separator line (except for the very first item — the status bar acts as separator).
    r->drawLine(0, yPos, DISPLAY_W, yPos, true /*always black*/);

    // ── Title ─────────────────────────────────────────────────────────────────
    int lineH_bold  = r->getLineHeight(FONT_BOLD);
    int lineH_small = r->getLineHeight(FONT_SMALL);

    // Vertical layout within the row:
    //   Top padding:  (ITEM_H - titleH - authorH - 4) / 2
    // When there's no author we centre the title alone.
    bool hasAuthor = (e.author[0] != '\0');
    int  textBlockH = lineH_bold + (hasAuthor ? (lineH_small + 2) : 0);
    int  textY      = yPos + (ITEM_H - textBlockH) / 2;

    // Right-side annotations (size, save indicator).
    // Build size string.
    char sizeStr[16];
    formatSize(e.sizeBytes, sizeStr, sizeof(sizeStr));
    int sizeW = r->getTextWidth(FONT_SMALL, sizeStr);

    // Save indicator: a checkmark if hasSave.
    // Use ASCII to keep it simple and avoid glyph-missing issues.
    const char* saveStr  = e.hasSave ? "[S]" : "";
    int         saveW    = e.hasSave ? r->getTextWidth(FONT_BOLD, saveStr) : 0;

    // Currently-loaded marker.
    const char* loadedStr = e.isCurrentlyLoaded ? "[*]" : "";
    int loadedW = e.isCurrentlyLoaded ? r->getTextWidth(FONT_SMALL, loadedStr) : 0;

    // Right-hand annotation x position: leave ITEM_MARGIN_X gap from right edge.
    int annotX = DISPLAY_W - ITEM_MARGIN_X - sizeW;

    // Draw size at right edge.
    r->drawText(FONT_SMALL, annotX, textY + (lineH_bold - lineH_small) / 2, sizeStr, ink);

    // Draw save indicator (bold) to the left of size.
    if (e.hasSave) {
        int svX = annotX - saveW - 6;
        r->drawText(FONT_BOLD, svX, textY, saveStr, ink);
    }

    // Draw currently-loaded marker below save indicator.
    if (e.isCurrentlyLoaded) {
        int ldX = DISPLAY_W - ITEM_MARGIN_X - loadedW;
        int ldY = textY + lineH_bold + 2;
        r->drawText(FONT_SMALL, ldX, ldY, loadedStr, ink);
    }

    // Available width for the title (leave room for right annotations).
    int maxTitleW = annotX - ITEM_MARGIN_X - 8
                    - (saveW > 0 ? saveW + 6 : 0);

    // Truncate title if too wide.
    std::string truncTitle = r->truncatedText(FONT_BOLD, e.title, maxTitleW);
    r->drawText(FONT_BOLD, ITEM_MARGIN_X, textY, truncTitle.c_str(), ink);

    // ── Author ────────────────────────────────────────────────────────────────
    if (hasAuthor) {
        int authorY = textY + lineH_bold + 2;
        int maxAuthorW = annotX - ITEM_MARGIN_X - 8;
        std::string truncAuthor = r->truncatedText(FONT_SMALL, e.author, maxAuthorW);
        r->drawText(FONT_SMALL, ITEM_MARGIN_X, authorY, truncAuthor.c_str(), ink);
    }
#else
    // Native: just print something to stdout for debugging.
    (void)index; (void)yPos; (void)selected;
#endif
}

// ─── renderEmpty() ────────────────────────────────────────────────────────────

void GameLibrary::renderEmpty() {
    auto* r = _display.getRenderer();
    if (!r) return;

#ifdef PLATFORM_ESP32
    static const char* kLine1 = "No stories found.";
    static const char* kLine2 = "Copy .bin files to";
    static const char* kLine3 = "/eenk/ on the SD card.";

    int lineH = r->getLineHeight(FONT_NORMAL);
    int totalH = lineH * 3 + 8;
    // Centre within the content area (between status bar and hint bar).
    int contentH = DISPLAY_H - STATUS_BAR_H - HINT_BAR_H;
    int startY = STATUS_BAR_H + (contentH - totalH) / 2;

    r->drawCenteredText(FONT_BOLD,   startY,              kLine1, true);
    r->drawCenteredText(FONT_NORMAL, startY + lineH + 4,  kLine2, true);
    r->drawCenteredText(FONT_NORMAL, startY + lineH * 2 + 8, kLine3, true);
#endif
}

// ─── renderHintBar() ──────────────────────────────────────────────────────────────────────────────

void GameLibrary::renderHintBar() {
    auto* r = _display.getRenderer();
    if (!r) return;

#ifdef PLATFORM_ESP32
    int barY = DISPLAY_H - HINT_BAR_H;

    // Background.
    r->fillRect(0, barY, DISPLAY_W, HINT_BAR_H, false /*white*/);
    // Top separator.
    r->drawLine(0, barY, DISPLAY_W, barY, true);

    // Hint entries: [ OK=Open ]  [ ↑↓=Scroll ]  [ Back=Settings ]
    // Draw as evenly-spaced labelled pill-buttons.
    struct Hint { const char* btn; const char* action; };
    static const Hint kHints[] = {
        { "OK",   "Open"     },
        { "^v",   "Scroll"   },
        { "Back", "Settings" },
    };
    static constexpr int kNumHints = 3;

    int smallH = r->getLineHeight(FONT_SMALL);
    int hintY  = barY + (HINT_BAR_H - smallH) / 2;
    int cellW  = DISPLAY_W / kNumHints;

    for (int i = 0; i < kNumHints; i++) {
        // Build label like "[OK] Open"
        char label[32];
        snprintf(label, sizeof(label), "[%s] %s", kHints[i].btn, kHints[i].action);
        int lw = r->getTextWidth(FONT_SMALL, label);
        int lx = i * cellW + (cellW - lw) / 2;
        r->drawText(FONT_SMALL, lx, hintY, label, true);
    }
#endif
}

// ─── render() ─────────────────────────────────────────────────────────────────

void GameLibrary::render() {
    auto* r = _display.getRenderer();
    if (!r) {
        // Text-mode fallback: print list to stdout.
        printf("=== EENK Game Library ===\n");
        for (int i = 0; i < _numEntries; i++) {
            printf("%s %s\n",
                   (i == _selectedIndex) ? ">" : " ",
                   _entries[i].title);
        }
        return;
    }

    _display.clear();
    renderStatusBar();

    if (_numEntries == 0) {
        renderEmpty();
    } else {
        // Render visible entries.
        int lastVisible = _scrollOffset + VISIBLE_ITEMS;
        if (lastVisible > _numEntries) lastVisible = _numEntries;

        for (int i = _scrollOffset; i < lastVisible; i++) {
            int row  = i - _scrollOffset;
            int yPos = STATUS_BAR_H + row * ITEM_H;
            renderEntry(i, yPos, i == _selectedIndex);
        }

#ifdef PLATFORM_ESP32
        // Scroll arrows.
        if (_scrollOffset > 0) {
            // Up arrow at top-right of list area.
            r->drawText(FONT_BOLD, DISPLAY_W / 2, STATUS_BAR_H + 2, "^", true);
        }
        if (_scrollOffset + VISIBLE_ITEMS < _numEntries) {
            // Down arrow at bottom of list area.
            int arrowY = STATUS_BAR_H + VISIBLE_ITEMS * ITEM_H - 14;
            r->drawText(FONT_BOLD, DISPLAY_W / 2, arrowY, "v", true);
        }
#endif
    }

    // First render: do a full (ghosting-clearing) refresh; subsequent renders
    // use a fast partial refresh to minimise flicker.
    renderHintBar();

    if (_firstRender) {
#ifdef PLATFORM_ESP32
        // Feed the task watchdog — fullRefresh() blocks for ~2 s.
        yield();
#endif
        _display.fullRefresh();
        _firstRender = false;
    } else {
        _display.present();
    }
}

// ─── launchStory() ────────────────────────────────────────────────────────────

void GameLibrary::launchStory(int index) {
    if (index < 0 || index >= _numEntries) return;

#ifdef PLATFORM_ESP32
    Serial.printf("[Library] Launching story: %s\n", _entries[index].path);
    // Release SPI peripherals so INK_RUNTIME boot can re-initialise them cleanly.
    SD.end();
    SPI.end();
#endif
    BootManager::setBootMode(BootMode::INK_RUNTIME);
    BootManager::setStoryPath(_entries[index].path);
    BootManager::reboot();
    // reboot() never returns on ESP32 (ESP.restart()).
    // On native it calls exit(0).
}

// ─── run() ────────────────────────────────────────────────────────────────────

bool GameLibrary::run() {
    ensureFonts();
    scanSD();
    render();

    while (true) {
        ButtonEvent ev = _input.pollInput();
        switch (ev) {
            case ButtonEvent::UP:
                if (_numEntries > 0) {
                    _selectedIndex--;
                    if (_selectedIndex < 0) _selectedIndex = _numEntries - 1;
                    clampScroll();
                    render();
                }
                break;

            case ButtonEvent::DOWN:
                if (_numEntries > 0) {
                    _selectedIndex++;
                    if (_selectedIndex >= _numEntries) _selectedIndex = 0;
                    clampScroll();
                    render();
                }
                break;

            case ButtonEvent::CONFIRM:
                if (_numEntries > 0) {
                    launchStory(_selectedIndex);
                    // launchStory reboots or exits; we never get here on ESP32.
                }
                break;

            case ButtonEvent::BACK:
            case ButtonEvent::QUIT:
                // Navigate to Settings.
                return true;

            case ButtonEvent::LEFT:
            case ButtonEvent::RIGHT:
                // Reserved: could be used for sorting/filtering in future.
                break;

            case ButtonEvent::SLEEP:
#ifdef PLATFORM_ESP32
                // Power off cleanly: no save needed from the library screen.
                {
                    SystemUI ui(_display);
                    ui.showSleepCover();
                }
                esp_deep_sleep_enable_gpio_wakeup(
                    1ULL << InputManager::POWER_BUTTON_PIN,
                    ESP_GPIO_WAKEUP_GPIO_LOW);
                esp_deep_sleep_start();
                // Never returns.
#endif
                break;


            default:
                // ButtonEvent::NONE and anything else — yield.
#ifdef PLATFORM_ESP32
                delay(10);
#endif
                break;
        }
    }
}
