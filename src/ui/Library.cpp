// eenk — Library implementation
// Scrollable story browser: scans /eenk/ on SD, shows title/author/size,
// launches selected story via BootManager.
#include "Library.h"
#include "BatteryWidget.h"
#include "ListItemWidget.h"
#include "LoadingWidget.h"
#include "StoryMetadata.h"
#include "SystemUI.h"
#include "os/BootManager.h"

#include "ImageWidget.h"
#include "NeuStyle.h"
#include "QuickMenuWidget.h"
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <cctype>
#include <cstdio>
#include <cstring>


#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE)
#ifdef PLATFORM_ESP32
#include "HalInit.h"
#include "HalTypes.h"
#include <Arduino.h>
#include <InputManager.h>
#include <SPI.h>
#include <esp_sleep.h>

#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#else
// ── Native / SDL simulation ────────────────────────────────────────────────
#include <dirent.h>
#include <sys/stat.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────

Library::Library(IDisplay &display, IInput &input, BatteryWidget &battery,
                 AppSettings &settings)
    : _display(display), _input(input), _battery(battery), _settings(settings) {
}

static uint32_t library_fnv1a_32(const char *str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= (unsigned char)*str++;
    hash *= 16777619u;
  }
  return hash;
}

void Library::parseThumbMetadata(Library::StoryEntry &e) {
  e.thumbOffset = 0;
  e.thumbSize = 0;
  e.thumbW = 0;
  e.thumbH = 0;

  char mediaPath[256];
  snprintf(mediaPath, sizeof(mediaPath), "%s", e.path);
  char *dot = strrchr(mediaPath, '.');
  if (dot) {
    strcpy(dot, ".media");
  } else {
    return;
  }

  auto file = SDCardManager::getInstance().openFile(mediaPath);
  if (!file)
    return;

  uint32_t magic = 0;
  if (file.read((uint8_t *)&magic, 4) != 4 || magic != 0x4D4B4E45) { // "ENKM"
    file.close();
    return;
  }

  uint32_t numEntries = 0;
  if (file.read((uint8_t *)&numEntries, 4) != 4) {
    file.close();
    return;
  }

  uint32_t targetHash = library_fnv1a_32("@thumbnail");

  for (uint32_t i = 0; i < numEntries; ++i) {
    uint32_t hash = 0, offset = 0, size = 0, w = 0, h = 0;
    if (file.read((uint8_t *)&hash, 4) != 4)
      break;
    if (file.read((uint8_t *)&offset, 4) != 4)
      break;
    if (file.read((uint8_t *)&size, 4) != 4)
      break;
    if (file.read((uint8_t *)&w, 4) != 4)
      break;
    if (file.read((uint8_t *)&h, 4) != 4)
      break;

    if (hash == targetHash) {
      e.thumbOffset = offset;
      e.thumbSize = size;
      e.thumbW = (uint16_t)w;
      e.thumbH = (uint16_t)h;
      break;
    }
  }

  file.close();
}

// ─── titleFromFilename()
// ──────────────────────────────────────────────────────

void Library::titleFromFilename(const char *filename, char *outTitle,
                                size_t outLen) {
  if (outLen == 0)
    return;

  // Copy filename, stripping ".bin" suffix (case-insensitive).
  size_t len = strlen(filename);
  if (len > 4) {
    const char *ext = filename + len - 4;
    if (ext[0] == '.' && (ext[1] == 'b' || ext[1] == 'B') &&
        (ext[2] == 'i' || ext[2] == 'I') && (ext[3] == 'n' || ext[3] == 'N')) {
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

// ─── titleFromPath()
// ──────────────────────────────────────────────────────────────

void Library::titleFromPath(const char *storyPath, char *outTitle,
                            size_t outLen) {
  if (outLen == 0 || !storyPath || !storyPath[0])
    return;

  const char *lastSlash = strrchr(storyPath, '/');
#ifdef _WIN32
  const char *lastBackslash = strrchr(storyPath, '\\');
  if (lastBackslash > lastSlash)
    lastSlash = lastBackslash;
#endif

  const char *filename = lastSlash ? lastSlash + 1 : storyPath;

  // Extract stem name
  char stem[128] = {};
  strncpy(stem, filename, sizeof(stem) - 1);
  char *dot = strrchr(stem, '.');
  if (dot)
    *dot = '\0';

  // Check if stem is generic (main, index, story)
  bool isGeneric =
      (strcasecmp(stem, "main") == 0 || strcasecmp(stem, "index") == 0 ||
       strcasecmp(stem, "story") == 0);

  if (isGeneric && lastSlash && lastSlash > storyPath) {
    const char *prevSlash = lastSlash - 1;
    while (prevSlash > storyPath && *prevSlash != '/' && *prevSlash != '\\') {
      prevSlash--;
    }
    const char *parentStart =
        (*prevSlash == '/' || *prevSlash == '\\') ? prevSlash + 1 : prevSlash;
    size_t parentLen = lastSlash - parentStart;
    if (parentLen > 0 && parentLen < outLen) {
      strncpy(outTitle, parentStart, parentLen);
      outTitle[parentLen] = '\0';

      for (size_t i = 0; i < parentLen; i++) {
        if (outTitle[i] == '_' || outTitle[i] == '-')
          outTitle[i] = ' ';
      }
      outTitle[0] = (char)toupper((unsigned char)outTitle[0]);
      return;
    }
  }

  titleFromFilename(filename, outTitle, outLen);
}

// ─── formatSize()
// ─────────────────────────────────────────────────────────────

void Library::formatSize(uint32_t bytes, char *out, size_t outLen) {
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

// ─── focusLoadedStory() ──────────────────────────────────────────────────────

void Library::focusLoadedStory() {
  for (int i = 0; i < _numEntries; i++) {
    if (_entries[i].isCurrentlyLoaded) {
      _selectedIndex = i;
      clampScroll();
      break;
    }
  }
}

// ─── sortEntries()
// ────────────────────────────────────────────────────────────

void Library::sortEntries() {
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

// ─── scanSD()
// ─────────────────────────────────────────────────────────────────

void Library::scanSD() {
  _numEntries = 0;
  memset(_entries, 0, sizeof(_entries));

  // Retrieve the currently-loaded story path from NVS so we can mark it.
  char currentPath[128] = {};
  BootManager::getStoryPath(currentPath, sizeof(currentPath));

#ifdef PLATFORM_ESP32
  // ── ESP32: read recursively from SD card ─────────────────────────────────
  auto scanDirRecursive = [&](auto self, const char *dirPath,
                              int depth) -> void {
    if (depth > 2 || _numEntries >= MAX_STORIES)
      return;

    File dir = SD_FS.open(dirPath);
    if (!dir || !dir.isDirectory())
      return;

    yield();
    File f = dir.openNextFile();
    while (f && _numEntries < MAX_STORIES) {
      String nameStr = f.name();
      const char *rawName = nameStr.c_str();

      const char *lastSlash = strrchr(rawName, '/');
      const char *baseName = lastSlash ? lastSlash + 1 : rawName;

      if (f.isDirectory()) {
        if (baseName[0] != '.' && strcasecmp(baseName, "fonts") != 0 &&
            strcasecmp(baseName, "system") != 0 &&
            strcasecmp(baseName, ".eenk_saves") != 0) {
          char subPath[256];
          if (rawName[0] == '/') {
            snprintf(subPath, sizeof(subPath), "%s", rawName);
          } else {
            snprintf(subPath, sizeof(subPath), "%s/%s", dirPath, baseName);
          }
          self(self, subPath, depth + 1);
        }
      } else {
        if (nameStr.endsWith(".bin") || nameStr.endsWith(".BIN")) {
          StoryEntry &e = _entries[_numEntries];

          if (rawName[0] == '/') {
            snprintf(e.path, sizeof(e.path), "%s", rawName);
          } else {
            snprintf(e.path, sizeof(e.path), "%s/%s", dirPath, baseName);
          }
          e.sizeBytes = (uint32_t)f.size();

          StoryMetadata meta;
          if (StoryMetadata::readFromSD(e.path, &meta)) {
            e.hasMetadata = true;
            strncpy(e.title, meta.title, sizeof(e.title) - 1);
            strncpy(e.author, meta.author, sizeof(e.author) - 1);
            e.title[sizeof(e.title) - 1] = '\0';
            e.author[sizeof(e.author) - 1] = '\0';

            if (e.title[0] == '\0') {
              titleFromPath(e.path, e.title, sizeof(e.title));
            }
          } else {
            e.hasMetadata = false;
            titleFromPath(e.path, e.title, sizeof(e.title));
          }

          char savePath[256];
          StoryMetadata::getSavePath(e.path, savePath, sizeof(savePath));
          e.hasSave = SD_FS.exists(savePath);

          e.isCurrentlyLoaded =
              (currentPath[0] != '\0' && strcmp(e.path, currentPath) == 0);

          parseThumbMetadata(e);

          _numEntries++;
        }
      }
      f = dir.openNextFile();
    }
    dir.close();
  };

  scanDirRecursive(scanDirRecursive, "/stories", 0);

#else
  // ── Native: scan local stories/ directory recursively ────────────────────
  auto scanNativeDirRecursive = [&](auto self, const char *dirPath,
                                    int depth) -> void {
    if (depth > 2 || _numEntries >= MAX_STORIES)
      return;

    DIR *dp = opendir(dirPath);
    if (!dp)
      return;

    struct dirent *ent;
    while ((ent = readdir(dp)) != nullptr && _numEntries < MAX_STORIES) {
      const char *name = ent->d_name;
      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || name[0] == '.')
        continue;

      char fullPath[256];
      snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, name);

      struct stat st;
      if (stat(fullPath, &st) != 0)
        continue;

      if (S_ISDIR(st.st_mode)) {
        if (strcasecmp(name, "fonts") != 0 && strcasecmp(name, "system") != 0 &&
            strcasecmp(name, ".eenk_saves") != 0) {
          self(self, fullPath, depth + 1);
        }
      } else if (S_ISREG(st.st_mode)) {
        size_t nlen = strlen(name);
        if (nlen >= 4) {
          const char *ext = name + nlen - 4;
          if (ext[0] == '.' && (ext[1] == 'b' || ext[1] == 'B') &&
              (ext[2] == 'i' || ext[2] == 'I') &&
              (ext[3] == 'n' || ext[3] == 'N')) {
            StoryEntry &e = _entries[_numEntries];
            strncpy(e.path, fullPath, sizeof(e.path) - 1);
            e.sizeBytes = (uint32_t)st.st_size;
            e.hasMetadata = false;
            titleFromPath(e.path, e.title, sizeof(e.title));
            e.author[0] = '\0';

            char savePath[256];
            StoryMetadata::getSavePath(e.path, savePath, sizeof(savePath));
            e.hasSave = false; // native stub
            e.isCurrentlyLoaded = false;

            parseThumbMetadata(e);

            _numEntries++;
          }
        }
      }
    }
    closedir(dp);
  };

  scanNativeDirRecursive(scanNativeDirRecursive, "stories", 0);
#endif

  sortEntries();
  focusLoadedStory();
}

// ─── clampScroll()
// ────────────────────────────────────────────────────────────

void Library::clampScroll() {
  // Ensure selected is within [0, _numEntries).
  if (_selectedIndex < 0)
    _selectedIndex = 0;
  if (_selectedIndex >= _numEntries)
    _selectedIndex = _numEntries > 0 ? _numEntries - 1 : 0;

  // Scroll up if selected is above the visible window.
  if (_selectedIndex < _scrollOffset) {
    _scrollOffset = _selectedIndex;
  }
  // Scroll down if selected is below the visible window.
  if (_selectedIndex >= _scrollOffset + VISIBLE_ITEMS) {
    _scrollOffset = _selectedIndex - VISIBLE_ITEMS + 1;
  }
  // Keep scrollOffset non-negative.
  if (_scrollOffset < 0)
    _scrollOffset = 0;
}

// ─── renderEntry()
// ────────────────────────────────────────────────────────────

void Library::renderEntry(int index, int yPos, bool selected) {
  auto *r = _display.getRenderer();
  if (!r)
    return;

#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE) ||                     \
    defined(PIO_UNIT_TESTING)
  const StoryEntry &e = _entries[index];

  int cardX = 24;
  int cardY = yPos + 4;
  int cardW = DISPLAY_W - 48;
  int cardH = ITEM_H - 8;

  ListItemWidget::draw(
      r, cardX, cardY, cardW, cardH, selected,
      [&](int inX, int inY, int inW, int inH) {
        bool ink = true; // Always black text on white background inside card

        // ── Layout
        int squareW = e.thumbSize > 0 ? inH : 0;
        int textStartX = inX + squareW + 16;
        int annotX = inX + inW - 8;

        // ── Left Side Square (Thumbnail)
        if (e.thumbSize > 0) {
          r->fillRect(inX, inY, squareW, inH, true);
          char mediaPath[256];
          snprintf(mediaPath, sizeof(mediaPath), "%s", e.path);
          char *dot = strrchr(mediaPath, '.');
          if (dot)
            strcpy(dot, ".media");

          auto file = SDCardManager::getInstance().openFile(mediaPath);
          if (file) {
            ImageWidget::draw(*r, file, e.thumbOffset, e.thumbSize, e.thumbW,
                              e.thumbH, inX + (squareW - e.thumbW) / 2,
                              inY + (inH - e.thumbH) / 2, e.thumbW, e.thumbH);
            file.close();
          }
        }

        // ── Title & Author Layout
        int lineH_bold = r->getLineHeight(FONT_BOLD);
        int lineH_small = r->getLineHeight(FONT_SMALL);
        bool hasAuthor = (e.author[0] != '\0');

        // Available width for the title
        int maxTitleW = annotX - textStartX;

        int maxTitleLines = e.thumbSize > 0 ? 3 : 2;

        auto titleLinesCheck = r->wrapTextWithHyphenation(
            FONT_BOLD, e.title, maxTitleW, maxTitleLines + 1);
        if (titleLinesCheck.empty()) {
          titleLinesCheck.push_back(e.title);
        }
        int titleLinesCount =
            std::min((int)titleLinesCheck.size(), maxTitleLines);

        int textBlockH = lineH_bold * titleLinesCount;
        if (hasAuthor)
          textBlockH += 2 + lineH_small;
        textBlockH += 8 + 16; // Padding + status line

        int textY = inY + (inH - textBlockH) / 2;
        int currentY = textY;

        for (int i = 0; i < titleLinesCount; ++i) {
          if (i == titleLinesCount - 1 &&
              titleLinesCheck.size() > (size_t)titleLinesCount) {
            std::string remainder = titleLinesCheck[i];
            for (size_t j = i + 1; j < titleLinesCheck.size(); ++j) {
              remainder += " " + titleLinesCheck[j];
            }
            std::string truncTitle =
                r->truncatedText(FONT_BOLD, remainder.c_str(), maxTitleW);
            r->drawText(FONT_BOLD, textStartX, currentY, truncTitle.c_str(),
                        ink);
          } else {
            r->drawText(FONT_BOLD, textStartX, currentY,
                        titleLinesCheck[i].c_str(), ink);
          }
          currentY += lineH_bold;
        }

        if (hasAuthor) {
          int authorY = currentY + 2;
          std::string truncAuthor =
              r->truncatedText(FONT_SMALL, e.author, maxTitleW);
          r->drawText(FONT_SMALL, textStartX, authorY, truncAuthor.c_str(),
                      ink);
          currentY = authorY + lineH_small;
        }

        // ── Status Line (Save icon, Size, Loaded)
        int statusY = currentY + 8;

        int currentX = textStartX;

        // Save indicator: 16x16 floppy icon if hasSave.
        if (e.hasSave) {
          static const uint8_t kFloppyIcon16[32] = {
              0xf0, 0x6c, 0xf0, 0x6e, 0xf0, 0x6f, 0xf0, 0x6f, 0xf0, 0x0f, 0xff,
              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x03, 0xd9, 0x2b,
              0xd2, 0xab, 0xcb, 0xab, 0xda, 0x93, 0xc0, 0x03, 0xff, 0xff};
          for (int fy = 0; fy < 16; fy++) {
            for (int fx_byte = 0; fx_byte < 2; fx_byte++) {
              uint8_t b = kFloppyIcon16[fy * 2 + fx_byte];
              for (int fx_bit = 0; fx_bit < 8; fx_bit++) {
                if (b & (1 << (7 - fx_bit))) {
                  r->drawPixel(currentX + fx_byte * 8 + fx_bit, statusY + fy,
                               ink);
                }
              }
            }
          }
          currentX += 16 + 8; // width + margin
        }

        // Build size string.
        char sizeStr[16];
        formatSize(e.sizeBytes, sizeStr, sizeof(sizeStr));
        int sizeW = r->getTextWidth(FONT_SMALL, sizeStr);
        int textOffsetY = (16 - lineH_small) / 2;

        r->drawText(FONT_SMALL, currentX, statusY + textOffsetY, sizeStr, ink);
        currentX += sizeW + 12;

        // Currently-loaded marker.
        if (e.isCurrentlyLoaded) {
          r->drawText(FONT_SMALL, currentX, statusY + textOffsetY, "[LOADED]",
                      ink);
        }
      });
#else
  // Native: just print something to stdout for debugging.
  (void)index;
  (void)yPos;
  (void)selected;
#endif
}

// ─── renderEmpty()
// ────────────────────────────────────────────────────────────

void Library::renderEmpty() {
  auto *r = _display.getRenderer();
  if (!r)
    return;

#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE) ||                     \
    defined(PIO_UNIT_TESTING)
  static const char *kLine1 = "No stories found.";
  static const char *kLine2 = "Copy .bin files to";
  static const char *kLine3 = "/stories/ on the SD card.";

  int lineH = r->getLineHeight(FONT_NORMAL);
  int totalH = lineH * 3 + 8;
  // Centre within the content area (between status bar and hint bar).
  int contentH = DISPLAY_H - HeaderWidget::HEIGHT - FooterWidget::HEIGHT;
  int startY = HeaderWidget::HEIGHT + (contentH - totalH) / 2;

  r->drawCenteredText(FONT_BOLD, startY, kLine1, true);
  r->drawCenteredText(FONT_NORMAL, startY + lineH + 4, kLine2, true);
  r->drawCenteredText(FONT_NORMAL, startY + lineH * 2 + 8, kLine3, true);
#endif
}

// ─── renderFooter()
// ──────────────────────────────────────────────────────────────────────────────

void Library::renderFooter() {
  auto *r = _display.getRenderer();
  if (!r)
    return;

  FooterWidget footer;
  footer.btnBack = {true, "OPTIONS", "Back", false};
  footer.btnConfirm = {true, "OPEN", "Confirm", true}; // Pill
  footer.btnPrev = {true, "PREV", "Prev", false};
  footer.btnNext = {true, "NEXT", "Next", false};

  footer.render(r, DISPLAY_W, DISPLAY_H);
}

// ─── render()
// ─────────────────────────────────────────────────────────────────

void Library::render() {
  auto *r = _display.getRenderer();
  if (!r) {
    // Text-mode fallback: print list to stdout.
    printf("=== eenk Library ===\n");
    for (int i = 0; i < _numEntries; i++) {
      printf("%s %s\n", (i == _selectedIndex) ? ">" : " ", _entries[i].title);
    }
    return;
  }

  _display.clear();
  HeaderWidget header(_display, _battery);
  header.render("eenk", NeuStyle::FONT_HEADING);

  if (_numEntries == 0) {
    renderEmpty();
  } else {
    // Render visible entries.
    int lastVisible = _scrollOffset + VISIBLE_ITEMS;
    if (lastVisible > _numEntries)
      lastVisible = _numEntries;

    for (int i = _scrollOffset; i < lastVisible; i++) {
      int row = i - _scrollOffset;
      int yPos = HeaderWidget::HEIGHT + 8 + row * ITEM_H;
      renderEntry(i, yPos, i == _selectedIndex);
    }

#ifdef PLATFORM_ESP32
    // Scroll arrows.
    if (_scrollOffset > 0) {
      // Up arrow at top-right of list area.
      r->drawText(FONT_BOLD, DISPLAY_W / 2, HeaderWidget::HEIGHT + 2, "^",
                  true);
    }
    if (_scrollOffset + VISIBLE_ITEMS < _numEntries) {
      // Down arrow at bottom-right of list area.
      int arrowY = HeaderWidget::HEIGHT + VISIBLE_ITEMS * ITEM_H - 14;
      r->drawText(FONT_BOLD, DISPLAY_W / 2, arrowY, "v", true);
    }
#endif
  }

  // First render: do a full (ghosting-clearing) refresh; subsequent renders
  // use a fast partial refresh to minimise flicker.
  renderFooter();

  if (_firstRender) {
#ifdef PLATFORM_ESP32
    // Feed the task watchdog — fullRefresh() blocks for ~2 s.
    yield();
#endif
    if (_needsFullRefresh) {
      _display.fullRefresh();
    } else {
      _display.present();
    }
    _firstRender = false;
  } else {
    _display.present();
  }
}

// ─── launchStory()
// ────────────────────────────────────────────────────────────

void Library::launchStory(int index) {
  if (index < 0 || index >= _numEntries)
    return;

#ifdef PLATFORM_ESP32
  Serial.printf("[Library] Launching story: %s\n", _entries[index].path);

  // Provide a visual cue that we registered the click before the ESP restarts
  bool isLoaded = _entries[index].isCurrentlyLoaded;
  const char *titleStr = isLoaded ? "Resuming story..." : "Loading story...";
  LoadingWidget::show(_display, titleStr, 1.0f);

  // Release SPI peripherals so INK_RUNTIME boot can re-initialise them cleanly.
  SD_FS.end();
  SPI.end();
#endif
  BootManager::setBootMode(BootMode::INK_RUNTIME);
  BootManager::setStoryPath(_entries[index].path);
  BootManager::reboot();
  // reboot() never returns on ESP32 (ESP.restart()).
  // On native it calls exit(0).
}

// ─── run()
// ────────────────────────────────────────────────────────────────────

bool Library::run() {
  scanSD();
  focusLoadedStory();
  render();

  while (true) {
    if (_backgroundTask && _backgroundTask()) {
      scanSD();
      focusLoadedStory();
      render();
    }

    SystemUI::checkBatteryAndShutdown(_battery, _display);

    ButtonEvent ev = _input.pollInput();

    // TODO: this logic is broken, let's reimplement a cleaner touch handling
    // for selecting stories. int touchX = -1, touchY = -1; if
    // (_input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0) {
    //   if (touchY >= 440) {
    //     if (touchX < 400) {
    //       return true; // Open settings
    //     } else if (_numEntries > 0) {
    //       launchStory(_selectedIndex);
    //     }
    //   } else if (_numEntries > 0) {
    //     int relY = touchY - 45;
    //     if (relY >= 0) {
    //       int clickedIdx = _scrollOffset + (relY / 54);
    //       if (clickedIdx >= 0 && clickedIdx < _numEntries) {
    //         if (_selectedIndex == clickedIdx) {
    //           launchStory(clickedIdx);
    //         } else {
    //           _selectedIndex = clickedIdx;
    //           render();
    //         }
    //       }
    //     }
    //   }
    // }

    if (ev == ButtonEvent::TOP_EDGE_SWIPE) {
      QuickMenuWidget qm(_display, _input, _battery, nullptr, _settings);
      QuickMenuAction action = qm.show();
      if (action == QuickMenuAction::OPEN_SETTINGS) {
        return true;
      }
      render();
      continue;
    }

    switch (ev) {
    case ButtonEvent::UP:
    case ButtonEvent::LEFT:
      if (_numEntries > 0) {
        _selectedIndex--;
        if (_selectedIndex < 0)
          _selectedIndex = _numEntries - 1;
        clampScroll();
        render();
      }
      break;

    case ButtonEvent::DOWN:
    case ButtonEvent::RIGHT:
      if (_numEntries > 0) {
        _selectedIndex++;
        if (_selectedIndex >= _numEntries)
          _selectedIndex = 0;
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

    case ButtonEvent::SLEEP:
#ifdef PLATFORM_ESP32
      // Power off cleanly: no save needed from the library screen.
      {
        SystemUI ui(_display);
        ui.showSleepCover();
      }
      HalInit::prepareForSleep();
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
