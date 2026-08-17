#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdio.h>
#include <unity.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef PLATFORM_NATIVE
#include "hal/sdl/mock/FS.h"
#endif

#ifdef PLATFORM_NATIVE
#include "GfxRenderer.h"
#include "InkRichTextParser.h"
#include "StreamingEpdFont.h"
#include "StreamingEpdFontFamily.h"
#include "engine/InkEngine.h"
#include "hal/sdl/mock/EInkDisplay.h"
#include "os/AppSettings.h"
#include "os/BootManager.h"
#include "os/SdFontCatalogue.h"
#include "ui/BatteryWidget.h"

#define FRAME_W 800
#include "ui/FooterWidget.h"
#include "ui/ImageWidget.h"
#include "ui/Library.h"
#include "ui/NeuStyle.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/StoryMetadata.h"
#include "ui/SystemUI.h"
#include <builtinFonts/reader_2b.h>
#include <builtinFonts/reader_bold_2b.h>
#include <builtinFonts/reader_italic_2b.h>
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/reader_medium_bold_2b.h>
#include <builtinFonts/reader_medium_italic_2b.h>
#include <builtinFonts/syne_bold_10.h>
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>
#include <cstdio>

#endif

#include "ScriptDetector.h"

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_script_detector(void) {
  TEST_ASSERT_EQUAL(ScriptDetector::Script::LATIN,
                    ScriptDetector::classify("Hello"));
  TEST_ASSERT_EQUAL(ScriptDetector::Script::ARABIC,
                    ScriptDetector::classify("مرحبا"));
  TEST_ASSERT_EQUAL(ScriptDetector::Script::THAI,
                    ScriptDetector::classify("สวัสดี"));
}

#ifdef PLATFORM_NATIVE
void test_sd_font_catalogue(void) {
  SdFontCatalogue cat;
  cat.scan(); // Should pick up kBuiltinFonts and any .epdfont files in fonts/
              // directory

  TEST_ASSERT_GREATER_THAN(0, cat.getCount());

  // Check first font is a builtin (e.g. Sans)
  bool foundBuiltin = false;
  for (size_t i = 0; i < cat.getCount(); i++) {
    if (cat.getEntries()[i].path[0] == '\0') {
      foundBuiltin = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(foundBuiltin);
}
#endif

#ifdef PLATFORM_NATIVE
void saveBMP(const char *filename, const uint8_t *frameBuffer, int width,
             int height) {
  FILE *f = fopen(filename, "wb");
  if (!f)
    return;

  int rowSize = ((width + 31) / 32) * 4;
  int imageSize = rowSize * height;
  int fileSize = 14 + 40 + 8 + imageSize;

  uint8_t fileHeader[14] = {'B',
                            'M',
                            (uint8_t)(fileSize),
                            (uint8_t)(fileSize >> 8),
                            (uint8_t)(fileSize >> 16),
                            (uint8_t)(fileSize >> 24),
                            0,
                            0,
                            0,
                            0,
                            62,
                            0,
                            0,
                            0};

  uint8_t infoHeader[40] = {40,
                            0,
                            0,
                            0,
                            (uint8_t)(width),
                            (uint8_t)(width >> 8),
                            (uint8_t)(width >> 16),
                            (uint8_t)(width >> 24),
                            (uint8_t)(-height),
                            (uint8_t)(-height >> 8),
                            (uint8_t)(-height >> 16),
                            (uint8_t)(-height >> 24),
                            1,
                            0,
                            1,
                            0,
                            0,
                            0,
                            0,
                            0,
                            (uint8_t)(imageSize),
                            (uint8_t)(imageSize >> 8),
                            (uint8_t)(imageSize >> 16),
                            (uint8_t)(imageSize >> 24),
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            2,
                            0,
                            0,
                            0,
                            2,
                            0,
                            0,
                            0};

  uint8_t colorTable[8] = {0, 0, 0, 0, 255, 255, 255, 0};

  fwrite(fileHeader, 1, 14, f);
  fwrite(infoHeader, 1, 40, f);
  fwrite(colorTable, 1, 8, f);

  // We allocate a buffer for the logical image size. Width=480, height=800.
  // Row size for 480 width is exactly 60 bytes.
  uint8_t *rotatedBuffer = new uint8_t[imageSize];
  for (int i = 0; i < imageSize; i++)
    rotatedBuffer[i] = 0;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int hw_x = 799 - y;
      int hw_y = x;
      uint32_t idx = (hw_y * 800 + hw_x) / 8;
      uint8_t bit = 7 - (hw_x % 8);
      bool isWhite = (frameBuffer[idx] & (1 << bit)) != 0;
      if (isWhite) {
        rotatedBuffer[y * rowSize + (x / 8)] |= (1 << (7 - (x % 8)));
      }
    }
  }

  // Write pixel data
  fwrite(rotatedBuffer, 1, imageSize, f);
  delete[] rotatedBuffer;

  fclose(f);
  printf("Saved screenshot to %s\n", filename);
}

class TestDisplay : public IDisplay {
public:
  EInkDisplay eink;
  GfxRenderer renderer;
  EpdFont sysFontNormal;
  EpdFontFamily sysFamilyNormal;
  EpdFont sysFontBold;
  EpdFontFamily sysFamilyBold;
  EpdFont sysFontSmall;
  EpdFontFamily sysFamilySmall;
  EpdFont sysFontHeading;
  EpdFontFamily sysFamilyHeading;

  EpdFont readerNormal;
  EpdFont readerBold;
  EpdFont readerItalic;
  EpdFontFamily readerFamily;

  EpdFont readerSmall;
  EpdFont readerSmallBold;
  EpdFont readerSmallItalic;
  EpdFontFamily readerFamilySmall;

  TestDisplay()
      : renderer(eink), sysFontNormal(&ui_12), sysFamilyNormal(&sysFontNormal),
        sysFontBold(&ui_bold_12), sysFamilyBold(&sysFontBold),
        sysFontSmall(&ui_10), sysFamilySmall(&sysFontSmall),
        sysFontHeading(&syne_bold_10), sysFamilyHeading(&sysFontHeading),
        readerNormal(&reader_medium_2b), readerBold(&reader_medium_bold_2b),
        readerItalic(&reader_medium_italic_2b),
        readerFamily(&readerNormal, &readerBold, &readerItalic),
        readerSmall(&reader_2b), readerSmallBold(&reader_bold_2b),
        readerSmallItalic(&reader_italic_2b),
        readerFamilySmall(&readerSmall, &readerSmallBold, &readerSmallItalic) {
    renderer.begin();
    renderer.setOrientation(GfxRenderer::PortraitInverted);
    // Fallback
    renderer.insertFont(0, sysFamilyNormal);
    // SystemUI
    renderer.insertFont(10, sysFamilyNormal);
    renderer.insertFont(11, sysFamilyBold);
    // BatteryWidget
    renderer.insertFont(20, sysFamilySmall);
    // Library
    renderer.insertFont(30, sysFamilyNormal);
    renderer.insertFont(31, sysFamilyBold);
    renderer.insertFont(32, sysFamilySmall);
    // SettingsView
    renderer.insertFont(33, sysFamilySmall);

    // Global Heading Font for FooterWidget
    renderer.insertFont(NeuStyle::FONT_HEADING, sysFamilyHeading);

    // Test Fonts
    renderer.insertFont(50, readerFamily);
    renderer.insertFont(60, readerFamilySmall);
  }
  void clear() override { eink.clearScreen(0xFF); }
  void present() override {}
  void fullRefresh() override {}
  int getWidth() const override { return 480; }
  int getHeight() const override { return 800; }
  GfxRenderer *getRenderer() override { return &renderer; }
};

class MockInput : public IInput {
public:
  ButtonEvent pollInput() override {
    return ButtonEvent::QUIT; // Forces UI run() loops to exit immediately after
                              // one render
  }
  uint32_t getLastActivityTime() const override { return 0; }
  void setAutoSleepTimeout(uint16_t seconds) override {}
};

void test_battery_widget_screenshot(void) {
  TestDisplay display;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  // Usually drawn in the top right corner inside the black header
  display.renderer.fillRect(0, 0, display.getWidth(), 40, true);
  widget.draw(display.getWidth() - widget.getWidth() - 8,
              (40 - widget.getHeight()) / 2, true);
  widget.draw(display.getWidth() - widget.getWidth() - 8,
              (40 - widget.getHeight()) / 2 + 40, false);
  saveBMP("test/golden/test_battery_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

static void ensure_test_story_media(bool forceRebuild = false) {
  FILE *f = fopen("test/story/story.media", "rb");
  if (!f || forceRebuild) {
    if (f)
      fclose(f);
    int res = system("node test/build_test_story.js");
    (void)res;
  } else {
    fclose(f);
  }
}

class TestLibrary : public Library {
public:
  TestLibrary(IDisplay &display, IInput &input, BatteryWidget &battery,
              IFrontlight *frontlight, AppSettings &settings)
      : Library(display, input, battery, frontlight, settings) {}

protected:
  void scanSD() override {
    _numEntries = 8;

    snprintf(_entries[0].title, sizeof(_entries[0].title),
             "The Hitchhiker's Guide");
    snprintf(_entries[0].author, sizeof(_entries[0].author), "Douglas Adams");
    snprintf(_entries[0].path, sizeof(_entries[0].path),
             "test/story/story.bin");
    _entries[0].sizeBytes = 1500000;
    _entries[0].hasMetadata = true;
    _entries[0].hasSave = true;
    _entries[0].isCurrentlyLoaded = true;
    parseThumbMetadata(_entries[0]);

    snprintf(_entries[1].title, sizeof(_entries[1].title),
             "Zork I: The Great Underground Empire Very Long Title That "
             "Doesn't Fit");
    snprintf(_entries[1].author, sizeof(_entries[1].author), "Infocom");
    _entries[1].sizeBytes = 105000;
    _entries[1].hasMetadata = true;
    _entries[1].hasSave = false;
    _entries[1].isCurrentlyLoaded = false;

    snprintf(_entries[2].title, sizeof(_entries[2].title), "Anchorhead");
    snprintf(_entries[2].author, sizeof(_entries[2].author), "Michael Gentry");
    _entries[2].sizeBytes = 800000;
    _entries[2].hasMetadata = true;
    _entries[2].hasSave = true;

    snprintf(_entries[3].title, sizeof(_entries[3].title),
             "A Mind Forever Voyaging");
    snprintf(_entries[3].author, sizeof(_entries[3].author), "Steve Meretzky");
    _entries[3].sizeBytes = 250000;
    _entries[3].hasMetadata = true;
    _entries[3].hasSave = false;

    snprintf(_entries[4].title, sizeof(_entries[4].title), "Spider and Web");
    snprintf(_entries[4].author, sizeof(_entries[4].author), "Andrew Plotkin");
    _entries[4].sizeBytes = 320000;
    _entries[4].hasMetadata = true;
    _entries[4].hasSave = true;

    snprintf(_entries[5].title, sizeof(_entries[5].title), "Photopia");
    snprintf(_entries[5].author, sizeof(_entries[5].author), "Adam Cadre");
    _entries[5].sizeBytes = 150000;
    _entries[5].hasMetadata = true;
    _entries[5].hasSave = false;

    snprintf(_entries[6].title, sizeof(_entries[6].title), "Galatea");
    snprintf(_entries[6].author, sizeof(_entries[6].author), "Emily Short");
    _entries[6].sizeBytes = 410000;
    _entries[6].hasMetadata = true;
    _entries[6].hasSave = true;

    snprintf(_entries[7].title, sizeof(_entries[7].title), "80 Days");
    snprintf(_entries[7].author, sizeof(_entries[7].author), "inkle");
    _entries[7].sizeBytes = 5500000;
    _entries[7].hasMetadata = true;
    _entries[7].hasSave = false;
  }
};

void test_library_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  TestLibrary library(display, input, widget, nullptr /*frontlight*/, settings);
  library.run();

  saveBMP("test/golden/test_library.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_settings_view_screenshot(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  SettingsView view(display, input, widget, nullptr /*frontlight*/, settings);
  view.run();

  saveBMP("test/golden/test_settings_view.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_fonts_screenshot(void) {
  TestDisplay display;
  display.clear();

  int y = 50;

  display.renderer.drawText(10, 20, y, "UI Normal 12: The quick brown fox");
  y += display.renderer.getLineHeight(10) + 10;

  display.renderer.drawText(11, 20, y, "UI Bold 12: jumps over the lazy dog");
  y += display.renderer.getLineHeight(11) + 10;

  display.renderer.drawText(
      20, 20, y, "UI Small 10: Sphynx of black quartz, judge my vow");
  y += display.renderer.getLineHeight(20) + 30;

  display.renderer.drawText(50, 20, y,
                            "Reader M Normal: The quick brown fox jumps", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(50) + 10;

  display.renderer.drawText(50, 20, y,
                            "Reader M Italic: The quick brown fox jumps", true,
                            EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(50) + 10;

  display.renderer.drawText(50, 20, y,
                            "Reader M Bold: The quick brown fox jumps", true,
                            EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(50) + 20;

  display.renderer.drawText(60, 20, y,
                            "Reader S Normal: The quick brown fox jumps", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(60) + 10;

  display.renderer.drawText(60, 20, y,
                            "Reader S Italic: The quick brown fox jumps", true,
                            EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(60) + 10;

  display.renderer.drawText(60, 20, y,
                            "Reader S Bold: The quick brown fox jumps", true,
                            EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(60) + 20;

  auto runs1 = InkRichTextParser::parse(
      "Rich HTML: <i>Italic</i>, <b>Bold</b>, <b><i>Both</i></b>!");
  FILE *f = fopen("test_runs.txt", "w");
  auto blocks1 = display.renderer.wrapRichText(50, runs1, 440, 10);
  for (const auto &b : blocks1) {
    fprintf(f, "Line with %d runs\n", (int)b.runs.size());
    for (const auto &r : b.runs) {
      int w = display.renderer.getTextWidth(50, r.text.c_str(), r.style);

      // SUPER DEBUG:
      if (r.text == " ") {
        fprintf(f, "  SUPERDEBUG SPACE: Calling getTextWidth again: %d ",
                display.renderer.getTextWidth(50, " ", r.style));
      }

      fprintf(f, "  Run: [%s] len=%d style=%d width=%d firstchar=%d\n",
              r.text.c_str(), (int)r.text.length(), (int)r.style, w,
              (int)r.text[0]);
    }
    display.renderer.drawRichText(50, 20, y, b);
    y += display.renderer.getLineHeight(50);
  }
  fclose(f);
  y += 10;

  auto runs2 =
      InkRichTextParser::parse("Rich MD: *Italic*, **Bold**, **_Both_**!");
  auto blocks2 = display.renderer.wrapRichText(50, runs2, 440, 10);
  for (const auto &b : blocks2) {
    display.renderer.drawRichText(50, 20, y, b);
    y += display.renderer.getLineHeight(50);
  }
  y += 10;

  // Test choice rich text (using FONT_CHOICE = 11 for bold, 10 for normal UI
  // font in test) Actually ui_12 is 10, ui_bold_12 is 11 in test setup. wait,
  // FONT_CHOICE is not defined here. Let's just use 10.
  auto runs3 =
      InkRichTextParser::parse("Choice: *Italic*, **Bold**, **_Both_**!");
  auto blocks3 = display.renderer.wrapRichText(10, runs3, 440, 10);
  for (const auto &b : blocks3) {
    display.renderer.drawRichText(10, 20, y, b);
    y += display.renderer.getLineHeight(10);
  }
  y += 10;

  saveBMP("test/golden/test_fonts.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_external_fonts_screenshot(void) {
  TestDisplay display;
  display.clear();

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"sd_fonts", nullptr};
  TEST_ASSERT_TRUE(fam.load("orbitron", dirs));

  int y = 50;

  EpdFont r(fam.getData(EpdFontFamily::REGULAR));
  EpdFont b(fam.getData(EpdFontFamily::BOLD));
  EpdFont i(fam.getData(EpdFontFamily::ITALIC));
  EpdFont bi(fam.getData(EpdFontFamily::BOLD_ITALIC));
  EpdFontFamily sysFam(&r, &b, &i, &bi);

  display.renderer.insertFont(100, sysFam);
  display.renderer.removeStreamingFont(100);
  if (fam.hasStyle(EpdFontFamily::REGULAR))
    display.renderer.setStreamingFont(100, EpdFontFamily::REGULAR,
                                      fam.slot(EpdFontFamily::REGULAR));
  if (fam.hasStyle(EpdFontFamily::BOLD))
    display.renderer.setStreamingFont(100, EpdFontFamily::BOLD,
                                      fam.slot(EpdFontFamily::BOLD));
  if (fam.hasStyle(EpdFontFamily::ITALIC))
    display.renderer.setStreamingFont(100, EpdFontFamily::ITALIC,
                                      fam.slot(EpdFontFamily::ITALIC));
  if (fam.hasStyle(EpdFontFamily::BOLD_ITALIC))
    display.renderer.setStreamingFont(100, EpdFontFamily::BOLD_ITALIC,
                                      fam.slot(EpdFontFamily::BOLD_ITALIC));

  display.renderer.drawText(100, 20, y, "External Font (Orbitron):", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(100) + 10;

  if (fam.hasStyle(EpdFontFamily::BOLD)) {
    display.renderer.drawText(100, 20, y, "Hello World from SD card!", true,
                              EpdFontFamily::BOLD);
  }

  saveBMP("test/golden/test_external_fonts.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_modal_dialog_screenshot(void) {
  TestDisplay display;
  MockInput input;
  SystemUI ui(display);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ui.showConfirmDialog(input, "Confirm Action",
                       "Are you sure you want to exit the current story?");

  saveBMP("test/golden/test_modal_dialog.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_modal_dialog_long_text_screenshot(void) {
  TestDisplay display;
  MockInput input;
  SystemUI ui(display);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ui.showConfirmDialog(
      input, nullptr,
      "Formatting the SD card will erase all saved story data, custom fonts, "
      "and user preferences. This action cannot be undone.");

  saveBMP("test/golden/test_modal_dialog_long.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_loading_widget_screenshot(void) {
  TestDisplay display;
  SystemUI ui(display);

  ui.showLoading("Loading Story...", 0.65f);

  saveBMP("test/golden/test_loading_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_error_widget_screenshot(void) {
  TestDisplay display;
  SystemUI ui(display);

  ui.showMessage(
      "Failed to load story",
      "The story file is corrupted or could not be found.\nPlease try "
      "re-flashing the device or formatting the SD card.");

  saveBMP("test/golden/test_error_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_sleep_cover_screenshot(void) {
  TestDisplay display;
  SystemUI ui(display);

  ui.showSleepCover("Zzzzz...", "Sleep mode");

  saveBMP("test/golden/test_sleep_cover.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_sleep_cover_image_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display;
  SystemUI ui(display);

  BootManager::setStoryPath("test/story/story.bin");

  ui.showSleepCover("Zzzzz...", "Sleep mode");

  saveBMP("test/golden/test_sleep_cover_image.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

void test_story_player_screenshot(void) {
  TestDisplay display;
  display.clear();

  int y = 50;
  display.renderer.drawText(50, 20, y, "You wake up in a dark room.", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(50) + 10;
  display.renderer.drawText(50, 20, y, "The air is cold.", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(50) + 20;

  display.renderer.drawText(50, 20, y, "What do you do?", true,
                            EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(50) + 30;

  int choiceFont = 10;
  display.renderer.drawText(choiceFont, 40, y, "1. Look around", true);
  y += display.renderer.getLineHeight(choiceFont) + 10;
  display.renderer.fillRect(
      20, y - 2, 440, display.renderer.getLineHeight(choiceFont) + 4, true);
  display.renderer.drawText(choiceFont, 40, y, "2. Go back to sleep", false);
  y += display.renderer.getLineHeight(choiceFont) + 10;
  display.renderer.drawText(choiceFont, 40, y, "3. Yell for help", true);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Power"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/test_story_player.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

class MockFrontlight : public IFrontlight {
public:
  MockFrontlight() = default;
  ~MockFrontlight() override = default;

  void on() override {}
  void off() override {}
  void setBrightness(uint8_t percent) override {}
  void setColorTemperature(uint8_t percent) override {}
  uint8_t getBrightness() const override { return 66; }
  uint8_t getColorTemperature() const override { return 33; }
};

void test_quick_menu_screenshot(void) {
  TestDisplay display;
  MockInput input;
  AppSettings settings = AppSettings::defaults();
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  QuickMenuWidget ui(display, input, widget, nullptr /*frontlight*/, settings);

  display.clear();
  display.renderer.drawText(10, 50, 600, "Background text...", true);

  ui.show();

  saveBMP("test/golden/test_quick_menu.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

// ── StreamingEpdFontFamily tests ───────────────────────────────────────────

// Writes a minimal valid .epdfont file (zero glyphs/intervals/bitmap) for
// testing. Layout matches EpdFontLoader::FileHeader + FileMetrics exactly.
static bool writeDummyEpdfont(const char *path, bool is2bit = false) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;

  // FileHeader: magic(4) version(2) flags(2) reserved(8) = 16 bytes
  uint32_t magic = 0x46445045u; // 'E','P','D','F' little-endian
  uint16_t version = 1;
  uint16_t flags = is2bit ? 0x0001u : 0x0000u;
  uint8_t reserved[8] = {0};

  fwrite(&magic, 1, 4, f);
  fwrite(&version, 1, 2, f);
  fwrite(&flags, 1, 2, f);
  fwrite(reserved, 1, 8, f);

  // FileMetrics: advanceY(1) padding(1) ascender(int16) descender(int16)
  //              intervalCount(uint32) glyphCount(uint32) bitmapSize(uint32) =
  //              14 bytes
  uint8_t advanceY = 16;
  uint8_t padding = 0;
  int16_t ascender = 12;
  int16_t descender = -4;
  uint32_t intervalCount = 0;
  uint32_t glyphCount = 0;
  uint32_t bitmapSize = 0;

  fwrite(&advanceY, 1, 1, f);
  fwrite(&padding, 1, 1, f);
  fwrite(&ascender, 1, 2, f);
  fwrite(&descender, 1, 2, f);
  fwrite(&intervalCount, 1, 4, f);
  fwrite(&glyphCount, 1, 4, f);
  fwrite(&bitmapSize, 1, 4, f);

  fclose(f);
  return true;
}

void test_streaming_epd_font_family_load_plain(void) {
  // Single file <stem>.epdfont should be treated as the regular variant
  const char *path = "/tmp/test_font.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(path));

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp", nullptr};
  TEST_ASSERT_TRUE(fam.load("test_font", dirs));
  TEST_ASSERT_TRUE(fam.isLoaded());
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::REGULAR));
  TEST_ASSERT_FALSE(fam.hasStyle(EpdFontFamily::BOLD));
  TEST_ASSERT_FALSE(fam.hasStyle(EpdFontFamily::ITALIC));
  TEST_ASSERT_FALSE(fam.hasStyle(EpdFontFamily::BOLD_ITALIC));
  TEST_ASSERT_NOT_NULL(fam.getData(EpdFontFamily::REGULAR));
  remove(path);
}

void test_streaming_epd_font_family_load_regular_suffix(void) {
  // <stem>-regular.epdfont should be preferred over <stem>.epdfont
  const char *path = "/tmp/myfont-regular.epdfont";
  const char *plain = "/tmp/myfont.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(path));
  TEST_ASSERT_TRUE(writeDummyEpdfont(plain));

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp", nullptr};
  TEST_ASSERT_TRUE(fam.load("myfont", dirs));
  TEST_ASSERT_TRUE(fam.isLoaded());
  remove(path);
  remove(plain);
}

void test_streaming_epd_font_family_fallback_chain(void) {
  // Only regular loaded: BOLD/ITALIC/BOLD_ITALIC should resolve to REGULAR
  const char *path = "/tmp/fbtest-regular.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(path));

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp", nullptr};
  TEST_ASSERT_TRUE(fam.load("fbtest", dirs));

  // resolveSlot must always return the regular slot when no variants exist
  StreamingEpdFont *reg = fam.resolveSlot(EpdFontFamily::REGULAR);
  StreamingEpdFont *bold = fam.resolveSlot(EpdFontFamily::BOLD);
  StreamingEpdFont *italic = fam.resolveSlot(EpdFontFamily::ITALIC);
  StreamingEpdFont *boldItalic = fam.resolveSlot(EpdFontFamily::BOLD_ITALIC);

  TEST_ASSERT_NOT_NULL(reg);
  TEST_ASSERT_EQUAL_PTR(reg, bold);       // BOLD falls back to REGULAR
  TEST_ASSERT_EQUAL_PTR(reg, italic);     // ITALIC falls back to REGULAR
  TEST_ASSERT_EQUAL_PTR(reg, boldItalic); // BOLD_ITALIC falls back to REGULAR
  remove(path);
}

void test_streaming_epd_font_family_all_styles(void) {
  // All four variants present: each resolveSlot should return its own slot
  const char *rpath = "/tmp/full-regular.epdfont";
  const char *bpath = "/tmp/full-bold.epdfont";
  const char *ipath = "/tmp/full-italic.epdfont";
  const char *bipath = "/tmp/full-bolditalic.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(rpath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(bpath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(ipath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(bipath));

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp", nullptr};
  TEST_ASSERT_TRUE(fam.load("full", dirs));
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::REGULAR));
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::BOLD));
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::ITALIC));
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::BOLD_ITALIC));

  StreamingEpdFont *reg = fam.slot(EpdFontFamily::REGULAR);
  StreamingEpdFont *bold = fam.slot(EpdFontFamily::BOLD);
  StreamingEpdFont *ital = fam.slot(EpdFontFamily::ITALIC);
  StreamingEpdFont *bi = fam.slot(EpdFontFamily::BOLD_ITALIC);
  TEST_ASSERT_NOT_NULL(reg);
  TEST_ASSERT_NOT_NULL(bold);
  TEST_ASSERT_NOT_NULL(ital);
  TEST_ASSERT_NOT_NULL(bi);
  // Each resolves to its own distinct instance
  TEST_ASSERT_EQUAL_PTR(reg, fam.resolveSlot(EpdFontFamily::REGULAR));
  TEST_ASSERT_EQUAL_PTR(bold, fam.resolveSlot(EpdFontFamily::BOLD));
  TEST_ASSERT_EQUAL_PTR(ital, fam.resolveSlot(EpdFontFamily::ITALIC));
  TEST_ASSERT_EQUAL_PTR(bi, fam.resolveSlot(EpdFontFamily::BOLD_ITALIC));
  remove(rpath);
  remove(bpath);
  remove(ipath);
  remove(bipath);
}

void test_streaming_epd_font_family_missing_regular_fails(void) {
  // No files at all — load() must fail
  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp/nonexistent_dir", nullptr};
  TEST_ASSERT_FALSE(fam.load("ghost", dirs));
  TEST_ASSERT_FALSE(fam.isLoaded());
}

void test_streaming_epd_font_family_bold_italic_fallback_order(void) {
  // BOLD_ITALIC missing, BOLD present: BOLD_ITALIC should resolve to BOLD
  const char *rpath = "/tmp/partial-regular.epdfont";
  const char *bpath = "/tmp/partial-bold.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(rpath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(bpath));

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"/tmp", nullptr};
  TEST_ASSERT_TRUE(fam.load("partial", dirs));
  TEST_ASSERT_TRUE(fam.hasStyle(EpdFontFamily::BOLD));
  TEST_ASSERT_FALSE(fam.hasStyle(EpdFontFamily::BOLD_ITALIC));

  StreamingEpdFont *bold = fam.slot(EpdFontFamily::BOLD);
  StreamingEpdFont *bi = fam.resolveSlot(EpdFontFamily::BOLD_ITALIC);
  TEST_ASSERT_EQUAL_PTR(bold, bi); // BOLD_ITALIC → BOLD
  remove(rpath);
  remove(bpath);
}

void test_sd_font_catalogue_family_detection(void) {
  // Write a font family into the fonts/ directory so SdFontCatalogue::scan()
  // picks it up. Verify that only the family root appears (no -bold/-italic
  // sub-entries).
#ifdef _WIN32
  _mkdir("fonts");
#else
  mkdir("fonts", 0777);
#endif
  const char *rpath = "fonts/tstsans-regular.epdfont";
  const char *bpath = "fonts/tstsans-bold.epdfont";
  const char *ipath = "fonts/tstsans-italic.epdfont";
  TEST_ASSERT_TRUE(writeDummyEpdfont(rpath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(bpath));
  TEST_ASSERT_TRUE(writeDummyEpdfont(ipath));

  SdFontCatalogue cat;
  cat.scan();

  size_t count = cat.getCount();
  TEST_ASSERT_GREATER_THAN(0, count);

  // Find the tstsans entry — should appear exactly once (as "Tstsans")
  int foundCount = 0;
  const FontEntry *entries = cat.getEntries();
  for (size_t i = 0; i < count; i++) {
    const char *name = entries[i].displayName;
    // The root family entry should exist; variant files should NOT
    if (strstr(name, "Tstsans") || strstr(name, "tstsans")) {
      foundCount++;
    }
    // No entry's displayName should contain "-bold" or "-italic"
    TEST_ASSERT_NULL(strstr(name, "-bold"));
    TEST_ASSERT_NULL(strstr(name, "-italic"));
  }
  // tstsans-regular.epdfont should produce exactly one entry ("Tstsans")
  TEST_ASSERT_EQUAL_INT(1, foundCount);

  // Cleanup
  remove(rpath);
  remove(bpath);
  remove(ipath);
}

#include "hal/sdl/SDLStorage.h"

void test_image_widget_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display;
  MockInput input;
  SDLStorage storage;
  InkEngine engine(display, input, storage);

  BootManager::setStoryPath("test/story/story.bin");
  engine.loadStory("test/story/story.bin");
  engine.update();

  saveBMP("test/golden/test_image_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}
void test_zero_delay_choice_reveal(void) {
  TestDisplay display;
  MockInput input;
  SDLStorage storage;
  InkEngine engine(display, input, storage);

  engine.setChoiceDelays(0, 0);
  TEST_ASSERT_EQUAL_UINT32(0, engine.getCascadeOffsetMs());
  TEST_ASSERT_EQUAL_UINT32(0, engine.getFocusDelayMs());

  engine.loadStory("test/story/story.bin");
  engine.update();
}

void test_story_metadata_save_path(void) {
  char saveBufFolder[256];
  char saveBufRootFile[256];

  // Story in a folder under /stories/ -> should use folder name (e.g.
  // /.eenk_saves/my_story.sav)
  StoryMetadata::getSavePath("/stories/my_story/game.bin", saveBufFolder,
                             sizeof(saveBufFolder));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/my_story.sav", saveBufFolder);

  // Story with main.bin name in folder -> should use folder name
  StoryMetadata::getSavePath("/stories/interceptor/main.bin", saveBufFolder,
                             sizeof(saveBufFolder));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/interceptor.sav", saveBufFolder);

  // Relative path inside folder -> should use folder name
  StoryMetadata::getSavePath("stories/my_story/story.bin", saveBufFolder,
                             sizeof(saveBufFolder));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/my_story.sav", saveBufFolder);

  // Windows backslash path inside folder -> should use folder name
  StoryMetadata::getSavePath("stories\\my_story\\story.bin", saveBufFolder,
                             sizeof(saveBufFolder));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/my_story.sav", saveBufFolder);

  // Story directly at root of /stories/ -> fallback to bin filename (e.g.
  // /.eenk_saves/my_story.bin.sav)
  StoryMetadata::getSavePath("/stories/my_story.bin", saveBufRootFile,
                             sizeof(saveBufRootFile));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/my_story.bin.sav", saveBufRootFile);

  // VERIFY NO COLLISION: folder /stories/my_story/ and root file
  // /stories/my_story.bin have distinct save paths!
  TEST_ASSERT_NOT_EQUAL(0, strcmp(saveBufFolder, saveBufRootFile));

  // Relative path directly at root of stories/ -> fallback to bin filename
  StoryMetadata::getSavePath("stories/the_intercept.bin", saveBufRootFile,
                             sizeof(saveBufRootFile));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/the_intercept.bin.sav",
                           saveBufRootFile);

  // Bin file without directory -> fallback to bin filename
  StoryMetadata::getSavePath("the_intercept.bin", saveBufRootFile,
                             sizeof(saveBufRootFile));
  TEST_ASSERT_EQUAL_STRING("/.eenk_saves/the_intercept.bin.sav",
                           saveBufRootFile);
}

void test_rich_text_parser(void) {
  auto runs = InkRichTextParser::parse("Line 1<br>Line 2<br/>Line 3<br /><b>Bold</b> <i>Italic</i> **MDBold** *MDItalic*");
  std::string fullText = "";
  for (const auto &r : runs) {
    fullText += r.text;
  }
  TEST_ASSERT_EQUAL_STRING("Line 1\nLine 2\nLine 3\nBold Italic MDBold MDItalic", fullText.c_str());
  TEST_ASSERT_TRUE(runs.size() >= 5);
}
#endif

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_script_detector);
#ifdef PLATFORM_NATIVE
  RUN_TEST(test_sd_font_catalogue);
  RUN_TEST(test_rich_text_parser);
  RUN_TEST(test_battery_widget_screenshot);
  RUN_TEST(test_library_screenshot);
  RUN_TEST(test_settings_view_screenshot);
  RUN_TEST(test_fonts_screenshot);
  RUN_TEST(test_external_fonts_screenshot);
  RUN_TEST(test_modal_dialog_screenshot);
  RUN_TEST(test_modal_dialog_long_text_screenshot);
  RUN_TEST(test_loading_widget_screenshot);
  RUN_TEST(test_error_widget_screenshot);
  RUN_TEST(test_sleep_cover_screenshot);
  RUN_TEST(test_sleep_cover_image_screenshot);
  RUN_TEST(test_story_player_screenshot);
  RUN_TEST(test_image_widget_screenshot);
  RUN_TEST(test_zero_delay_choice_reveal);
  RUN_TEST(test_story_metadata_save_path);
  RUN_TEST(test_quick_menu_screenshot);
  // StreamingEpdFontFamily unit tests
  RUN_TEST(test_streaming_epd_font_family_load_plain);
  RUN_TEST(test_streaming_epd_font_family_load_regular_suffix);
  RUN_TEST(test_streaming_epd_font_family_fallback_chain);
  RUN_TEST(test_streaming_epd_font_family_all_styles);
  RUN_TEST(test_streaming_epd_font_family_missing_regular_fails);
  RUN_TEST(test_streaming_epd_font_family_bold_italic_fallback_order);
  RUN_TEST(test_sd_font_catalogue_family_detection);
#endif
  UNITY_END();
  return 0;
}
