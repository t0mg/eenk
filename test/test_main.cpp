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
#include "book/BookEngine.h"
#include "engine/InkEngine.h"
#include "hal/sdl/mock/EInkDisplay.h"
#include "os/AppSettings.h"
#include "os/BootManager.h"
#include "os/SdFontCatalogue.h"
#include "ui/BatteryWidget.h"

#define FRAME_W 800
#include "engine/StorySaveManager.h"
#include "ui/FooterWidget.h"
#include "ui/ImageWidget.h"
#include "ui/Library.h"
#include "ui/MenuModalWidget.h"
#include "ui/ModalDialogWidget.h"
#include "ui/NeuStyle.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/StoryMetadata.h"
#include "ui/SystemUI.h"
#include <builtinFonts/literata_medium_2b.h>
#include <builtinFonts/literata_medium_bold_2b.h>
#include <builtinFonts/literata_medium_italic_2b.h>
#include <builtinFonts/literata_small_2b.h>
#include <builtinFonts/literata_small_bold_2b.h>
#include <builtinFonts/literata_small_italic_2b.h>
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
  std::string fn(filename);
  size_t lastSlash = fn.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    std::string dir = fn.substr(0, lastSlash);
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
  }

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

  uint8_t *rotatedBuffer = new uint8_t[imageSize];
  for (int i = 0; i < imageSize; i++)
    rotatedBuffer[i] = 0;

  int panelWidth = height; // In PortraitInverted: physical panel width = height
  int panelWidthBytes = (panelWidth + 7) / 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int hw_x = (panelWidth - 1) - y;
      int hw_y = x;
      uint32_t idx = hw_y * panelWidthBytes + (hw_x / 8);
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
  int _width;
  int _height;
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

  EpdFont literataNormal;
  EpdFont literataBold;
  EpdFont literataItalic;
  EpdFontFamily literataFamily;

  EpdFont literataSmallNormal;
  EpdFont literataSmallBold;
  EpdFont literataSmallItalic;
  EpdFontFamily literataSmallFamily;

  explicit TestDisplay(int w = 480, int h = 800)
      : _width(w), _height(h), eink(h, w), renderer(eink),
        sysFontNormal(&ui_12), sysFamilyNormal(&sysFontNormal),
        sysFontBold(&ui_bold_12), sysFamilyBold(&sysFontBold),
        sysFontSmall(&ui_10), sysFamilySmall(&sysFontSmall),
        sysFontHeading(&syne_bold_10), sysFamilyHeading(&sysFontHeading),
        readerNormal(&reader_medium_2b), readerBold(&reader_medium_bold_2b),
        readerItalic(&reader_medium_italic_2b),
        readerFamily(&readerNormal, &readerBold, &readerItalic),
        readerSmall(&reader_2b), readerSmallBold(&reader_bold_2b),
        readerSmallItalic(&reader_italic_2b),
        readerFamilySmall(&readerSmall, &readerSmallBold, &readerSmallItalic),
        literataNormal(&literata_medium_2b),
        literataBold(&literata_medium_bold_2b),
        literataItalic(&literata_medium_italic_2b),
        literataFamily(&literataNormal, &literataBold, &literataItalic),
        literataSmallNormal(&literata_small_2b),
        literataSmallBold(&literata_small_bold_2b),
        literataSmallItalic(&literata_small_italic_2b),
        literataSmallFamily(&literataSmallNormal, &literataSmallBold,
                            &literataSmallItalic) {
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
    renderer.insertFont(70, literataFamily);
    renderer.insertFont(71, literataSmallFamily);
  }
  void clear() override { eink.clearScreen(0xFF); }
  void present() override {}
  void fullRefresh() override {}
  int getWidth() const override { return _width; }
  int getHeight() const override { return _height; }
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
  void resetActivityTimer() override {}
};

class QueueMockInput : public IInput {
public:
  std::vector<ButtonEvent> queue;
  size_t index = 0;
  mutable int tapX = -1;
  mutable int tapY = -1;
  mutable bool hasTap = false;

  void push(ButtonEvent ev) { queue.push_back(ev); }
  void pushTap(int x, int y) {
    tapX = x;
    tapY = y;
    hasTap = true;
  }
  ButtonEvent pollInput() override {
    if (index < queue.size()) {
      return queue[index++];
    }
    return ButtonEvent::NONE;
  }
  bool getTouchPosition(int &x, int &y) const override {
    if (hasTap) {
      x = tapX;
      y = tapY;
      hasTap = false;
      return true;
    }
    x = -1;
    y = -1;
    return false;
  }
  uint32_t getLastActivityTime() const override { return 0; }
  void setAutoSleepTimeout(uint16_t seconds) override {}
  void resetActivityTimer() override {}
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

class TestLibraryWithEpub : public Library {
public:
  TestLibraryWithEpub(IDisplay &display, IInput &input, BatteryWidget &battery,
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
    _entries[0].contentType = StoryEntry::ContentType::INK_STORY;
    parseThumbMetadata(_entries[0]);

    snprintf(_entries[1].title, sizeof(_entries[1].title),
             "Dr Jekyll and Mr Hyde");
    snprintf(_entries[1].author, sizeof(_entries[1].author), "R. L. Stevenson");
    snprintf(_entries[1].path, sizeof(_entries[1].path),
             "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub");
    _entries[1].sizeBytes = 145000;
    _entries[1].hasMetadata = false;
    _entries[1].hasSave = false;
    _entries[1].isCurrentlyLoaded = false;
    _entries[1].contentType = StoryEntry::ContentType::EPUB_BOOK;
    parseEpubThumbCache(_entries[1]);

    snprintf(_entries[2].title, sizeof(_entries[2].title), "Anchorhead");
    snprintf(_entries[2].author, sizeof(_entries[2].author), "Michael Gentry");
    _entries[2].sizeBytes = 800000;
    _entries[2].hasMetadata = true;
    _entries[2].hasSave = true;
    _entries[2].contentType = StoryEntry::ContentType::INK_STORY;

    snprintf(_entries[3].title, sizeof(_entries[3].title),
             "A Mind Forever Voyaging");
    snprintf(_entries[3].author, sizeof(_entries[3].author), "Steve Meretzky");
    _entries[3].sizeBytes = 250000;
    _entries[3].hasMetadata = true;
    _entries[3].hasSave = false;
    _entries[3].contentType = StoryEntry::ContentType::INK_STORY;

    snprintf(_entries[4].title, sizeof(_entries[4].title), "Spider and Web");
    snprintf(_entries[4].author, sizeof(_entries[4].author), "Andrew Plotkin");
    _entries[4].sizeBytes = 320000;
    _entries[4].hasMetadata = true;
    _entries[4].hasSave = true;
    _entries[4].contentType = StoryEntry::ContentType::INK_STORY;

    snprintf(_entries[5].title, sizeof(_entries[5].title), "Photopia");
    snprintf(_entries[5].author, sizeof(_entries[5].author), "Adam Cadre");
    _entries[5].sizeBytes = 150000;
    _entries[5].hasMetadata = true;
    _entries[5].hasSave = false;
    _entries[5].contentType = StoryEntry::ContentType::INK_STORY;

    snprintf(_entries[6].title, sizeof(_entries[6].title), "Galatea");
    snprintf(_entries[6].author, sizeof(_entries[6].author), "Emily Short");
    _entries[6].sizeBytes = 410000;
    _entries[6].hasMetadata = true;
    _entries[6].hasSave = true;
    _entries[6].contentType = StoryEntry::ContentType::INK_STORY;

    snprintf(_entries[7].title, sizeof(_entries[7].title), "80 Days");
    snprintf(_entries[7].author, sizeof(_entries[7].author), "inkle");
    _entries[7].sizeBytes = 5500000;
    _entries[7].hasMetadata = true;
    _entries[7].hasSave = false;
    _entries[7].contentType = StoryEntry::ContentType::INK_STORY;
  }
};

void test_library_with_epub(void) {
  ensure_test_story_media();

  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  {
    BookEngine engine(display, input);
    engine.applySettings(settings);
    engine.loadBook("test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub");
    engine.update(); // Generates thumb.bin
  }

  TestLibraryWithEpub library(display, input, widget, nullptr /*frontlight*/,
                              settings);
  library.run();

  saveBMP("test/golden/test_library_with_epub.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
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

  int y = 40;

  display.renderer.drawText(10, 20, y, "UI Normal 12: The quick brown fox");
  y += display.renderer.getLineHeight(10) + 8;

  display.renderer.drawText(11, 20, y, "UI Bold 12: jumps over the lazy dog");
  y += display.renderer.getLineHeight(11) + 8;

  display.renderer.drawText(
      20, 20, y, "UI Small 10: Sphynx of black quartz, judge my vow");
  y += display.renderer.getLineHeight(20) + 16;

  // Sans Medium
  display.renderer.drawText(50, 20, y,
                            "Sans M Regular: The quick brown fox jumps", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(50) + 6;

  display.renderer.drawText(50, 20, y,
                            "Sans M Italic: The quick brown fox jumps", true,
                            EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(50) + 6;

  display.renderer.drawText(50, 20, y, "Sans M Bold: The quick brown fox jumps",
                            true, EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(50) + 16;

  // Literata Serif Medium
  display.renderer.drawText(70, 20, y,
                            "Literata M Regular: The quick brown fox jumps",
                            true, EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(70) + 6;

  display.renderer.drawText(70, 20, y,
                            "Literata M Italic: The quick brown fox jumps",
                            true, EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(70) + 6;

  display.renderer.drawText(70, 20, y,
                            "Literata M Bold: The quick brown fox jumps", true,
                            EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(70) + 16;

  // Rich HTML & MD with Literata
  auto runs1 = InkRichTextParser::parse(
      "Literata HTML: <i>Italic</i>, <b>Bold</b>, <b><i>Both</i></b>!");
  auto blocks1 = display.renderer.wrapRichText(70, runs1, 440, 10);
  for (const auto &b : blocks1) {
    display.renderer.drawRichText(70, 20, y, b);
    y += display.renderer.getLineHeight(70);
  }
  y += 6;

  auto runs2 =
      InkRichTextParser::parse("Literata MD: *Italic*, **Bold**, **_Both_**!");
  auto blocks2 = display.renderer.wrapRichText(70, runs2, 440, 10);
  for (const auto &b : blocks2) {
    display.renderer.drawRichText(70, 20, y, b);
    y += display.renderer.getLineHeight(70);
  }
  y += 10;

  // Choice rich text (UI 12)
  auto runs3 =
      InkRichTextParser::parse("Choice UI: *Italic*, **Bold**, **_Both_**!");
  auto blocks3 = display.renderer.wrapRichText(10, runs3, 440, 10);
  for (const auto &b : blocks3) {
    display.renderer.drawRichText(10, 20, y, b);
    y += display.renderer.getLineHeight(10);
  }

  saveBMP("test/golden/test_fonts.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_external_fonts_screenshot(void) {
  TestDisplay display;
  display.clear();

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"test/fonts", "sd_fonts", nullptr};
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
  int iconSize = 10;
  display.renderer.drawText(choiceFont, 40, y, "1. Look around", true);
  y += display.renderer.getLineHeight(choiceFont) + 10;
  display.renderer.fillRect(
      20, y - 2, 440, display.renderer.getLineHeight(choiceFont) + 4, true);
  display.renderer.drawTriangleIcon(
      24, y + (display.renderer.getLineHeight(choiceFont) - iconSize) / 2,
      iconSize, false);
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

void test_epub_page_render(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // Call update to render the first page
  engine.update();

  saveBMP("test/golden/test_epub_page_render.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_page_2_render(void) {
  TestDisplay display;
  QueueMockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // First page render (cover)
  engine.update();

  // Advance to page 2 (text page)
  input.push(ButtonEvent::DOWN);
  engine.update();

  saveBMP("test/golden/test_epub_page_2_render.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_exit_modal_render(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // First page render (cover)
  engine.update();

  // Show exit modal with progress percentage and battery widget
  char progressStr[32];
  snprintf(progressStr, sizeof(progressStr), "%d%%",
           engine.getProgressPercentage());
  ModalDialogWidget::show(display, input, &widget, "Exit Reader",
                          "Do you want to exit to the menu?", progressStr);

  saveBMP("test/golden/test_epub_exit_modal.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_bookmark_resume(void) {
  TestDisplay display1;
  QueueMockInput input1;
  AppSettings settings = AppSettings::defaults();

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";

  // Step 1: Open book and navigate to page 2 (chapter text)
  {
    BookEngine engine1(display1, input1);
    engine1.applySettings(settings);
    TEST_ASSERT_TRUE(engine1.loadBook(epubPath));
    engine1.update(); // Page 1 (cover)

    input1.push(ButtonEvent::DOWN);
    engine1.update(); // Advance to Page 2

    // Wait past debounce
    delay(350);

    // Save progress on sleep
    input1.push(ButtonEvent::SLEEP);
    engine1.update();
    TEST_ASSERT_TRUE(engine1.shouldSleep());
  }

  // Step 2: Open book again in a new engine instance, verify it resumes at page
  // 2
  {
    TestDisplay display2;
    MockInput input2;
    BookEngine engine2(display2, input2);
    engine2.applySettings(settings);
    TEST_ASSERT_TRUE(engine2.loadBook(epubPath));
    engine2.update();

    // Verify progress is preserved past cover (progress > 0)
    int pct = engine2.getProgressPercentage();
    TEST_ASSERT_TRUE_MESSAGE(pct > 0,
                             "Expected progress percentage > 0 on resume");
  }

  // Clean up test save file
  remove(".eenk_saves/the-strange-case-of-dr-jekyll-and-mr-hyde.sav");
}

void test_epub_framebuffer_cache(void) {
  TestDisplay display;
  MockInput input;
  AppSettings settings = AppSettings::defaults();

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";

  // Step 1: Render cover page (creates framebuffer cache and thumb.bin)
  {
    BookEngine engine(display, input);
    engine.applySettings(settings);
    TEST_ASSERT_TRUE(engine.loadBook(epubPath));
    engine.update(); // Render cover
  }

  // Step 2: Verify thumbnail exists
  char thumbPath[256];
  TEST_ASSERT_TRUE(
      BookEngine::getCoverThumbPath(epubPath, thumbPath, sizeof(thumbPath)));
  FILE *fThumb = fopen(thumbPath, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(fThumb, "Cover thumbnail file was not created");
  if (fThumb) {
    uint16_t w = 0, h = 0;
    uint32_t res = 0;
    TEST_ASSERT_EQUAL(1, fread(&w, 2, 1, fThumb));
    TEST_ASSERT_EQUAL(1, fread(&h, 2, 1, fThumb));
    TEST_ASSERT_EQUAL(1, fread(&res, 4, 1, fThumb));
    TEST_ASSERT_TRUE(w > 0 && w <= 152);
    TEST_ASSERT_TRUE(h > 0 && h <= 152);
    fclose(fThumb);
  }

  // Step 3: Re-render with a second engine - should hit cache
  {
    TestDisplay display2;
    MockInput input2;
    BookEngine engine2(display2, input2);
    engine2.applySettings(settings);
    TEST_ASSERT_TRUE(engine2.loadBook(epubPath));
    engine2.update(); // Hits cached framebuffer

    // Check framebuffer matches
    TEST_ASSERT_EQUAL_UINT8_ARRAY(display.eink.getFrameBuffer(),
                                  display2.eink.getFrameBuffer(),
                                  800 * 480 / 8);
  }
}

void test_epub_touch_disabled_navigation(void) {
  TestDisplay display;
  QueueMockInput input;
  AppSettings settings = AppSettings::defaults();
  settings.touchScrollEnabled = false; // Disable touch navigation

  BookEngine engine(display, input);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  TEST_ASSERT_TRUE(engine.loadBook(epubPath));
  engine.update(); // Page 1 (cover)

  // Simulate a touch tap on the right side of the screen
  input.pushTap(400, 400);
  engine.update();

  // Progress should still be 0 because touch is disabled
  TEST_ASSERT_EQUAL(0, engine.getProgressPercentage());

  // Now press physical DOWN button -> should advance
  delay(350);
  input.push(ButtonEvent::DOWN);
  engine.update();

  // Should have advanced to page 2 (progress > 0)
  TEST_ASSERT_TRUE(engine.getProgressPercentage() > 0);
}

void test_choice_states_screenshot(void) {
  TestDisplay display;
  display.clear();

  int marginX = 24;
  int width = display.getWidth();
  int narrativeWidth = width - (2 * marginX);
  int indicatorWidth = 24;

  int y = 45;
  int narrativeFont = 50; // Sans Medium

  // Narrative paragraphs with proper rich-text wrapping
  auto narrativeRuns = InkRichTextParser::parse(
      "You stand at the threshold of the forgotten vault. A cold whisper "
      "echoes from the darkness below, asking what path you will choose.");
  auto narrativeBlocks = display.renderer.wrapRichText(
      narrativeFont, narrativeRuns, narrativeWidth, 10);
  for (const auto &b : narrativeBlocks) {
    display.renderer.drawRichText(narrativeFont, marginX, y, b);
    y += display.renderer.getLineHeight(narrativeFont);
  }
  y += 15;

  // Separator divider
  display.renderer.fillRect(marginX, y, narrativeWidth, 2, true);
  y += 20;

  int choiceFont = 10; // UI 12
  int choiceLineHeight = display.renderer.getLineHeight(choiceFont);
  int choicePadding = choiceLineHeight / 3;
  int choiceContentWidth = narrativeWidth - indicatorWidth;

  auto drawChoiceItem = [&](const char *text, bool focused) {
    auto runs = InkRichTextParser::parse(text);
    auto blocks =
        display.renderer.wrapRichText(choiceFont, runs, choiceContentWidth, 10);
    int numLines = std::max((size_t)1, blocks.size());
    int textHeight = numLines * choiceLineHeight;
    // Button mode: 4px top + 4px bottom padding (8px total)
    int blockHeight = textHeight + 8;
    int verticalOffset = 4;

    if (focused) {
      display.renderer.fillRect(marginX - 4, y, narrativeWidth + 8, blockHeight,
                                true);
    }

    bool textBlack = !focused;
    int iconSize = 10;
    int iconX = marginX + 4;
    int textY = y + verticalOffset;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (i == 0 && focused) {
        display.renderer.drawTriangleIcon(
            iconX, textY + (choiceLineHeight - iconSize) / 2, iconSize,
            textBlack);
      }
      display.renderer.drawRichText(choiceFont, marginX + indicatorWidth, textY,
                                    blocks[i], textBlack);
      textY += choiceLineHeight;
    }
    y += blockHeight + choicePadding;
  };

  drawChoiceItem("Examine the runes on the archway", false);
  drawChoiceItem("Step forward with your **drawn blade**", false);
  drawChoiceItem("Turn back and <i>seal the heavy gate</i>", true);
  drawChoiceItem("Search the stone walls for hidden mechanisms", false);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Back"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/test_choices.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_choice_states_touch_screenshot(void) {
  TestDisplay display;
  display.clear();

  int marginX = 24;
  int width = display.getWidth();
  int narrativeWidth = width - (2 * marginX);
  int indicatorWidth = 24;

  int y = 45;
  int narrativeFont = 50; // Sans Medium

  auto narrativeRuns = InkRichTextParser::parse(
      "You stand at the threshold of the forgotten vault. A cold whisper "
      "echoes from the darkness below, asking what path you will choose.");
  auto narrativeBlocks = display.renderer.wrapRichText(
      narrativeFont, narrativeRuns, narrativeWidth, 10);
  for (const auto &b : narrativeBlocks) {
    display.renderer.drawRichText(narrativeFont, marginX, y, b);
    y += display.renderer.getLineHeight(narrativeFont);
  }
  y += 15;

  // Separator divider
  display.renderer.fillRect(marginX, y, narrativeWidth, 2, true);
  y += 20;

  int choiceFont = 10; // UI 12
  int choiceLineHeight = display.renderer.getLineHeight(choiceFont);
  int choicePadding = choiceLineHeight / 3;
  int choiceContentWidth = narrativeWidth - indicatorWidth;

  auto drawChoiceItem = [&](const char *text, bool focused) {
    auto runs = InkRichTextParser::parse(text);
    auto blocks =
        display.renderer.wrapRichText(choiceFont, runs, choiceContentWidth, 10);
    int numLines = std::max((size_t)1, blocks.size());
    int textHeight = numLines * choiceLineHeight;
    // Touch mode: 64px min-height
    int blockHeight = std::max(64, textHeight);
    int verticalOffset = (blockHeight - textHeight) / 2;

    if (focused) {
      display.renderer.fillRect(marginX - 4, y, narrativeWidth + 8, blockHeight,
                                true);
    }

    bool textBlack = !focused;
    int iconSize = 10;
    int iconX = marginX + 4;
    int textY = y + verticalOffset;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (i == 0 && focused) {
        display.renderer.drawTriangleIcon(
            iconX, textY + (choiceLineHeight - iconSize) / 2, iconSize,
            textBlack);
      }
      display.renderer.drawRichText(choiceFont, marginX + indicatorWidth, textY,
                                    blocks[i], textBlack);
      textY += choiceLineHeight;
    }
    y += blockHeight + choicePadding;
  };

  drawChoiceItem("Examine the runes on the archway", true);
  drawChoiceItem("Step forward with your **drawn blade**", false);
  drawChoiceItem("Turn back and <i>seal the heavy gate</i>", false);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Back"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/test_choices_touch.bmp", display.eink.getFrameBuffer(),
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

void test_quick_menu_disabled_choices_screenshot(void) {
  TestDisplay display;
  MockInput input;
  AppSettings settings = AppSettings::defaults();
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  QuickMenuWidget ui(display, input, widget, nullptr /*frontlight*/, settings,
                     /*choicesEnabled=*/false);

  display.clear();
  display.renderer.drawText(10, 50, 600, "Background text...", true);

  ui.show();

  saveBMP("test/golden/test_quick_menu_book_mode.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());

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
void test_choice_reveal_interaction(void) {
  TestDisplay display;
  MockInput input;
  SDLStorage storage;
  InkEngine engine(display, input, storage);

  AppSettings settings = AppSettings::defaults();
  engine.applySettings(settings);

  InkDisplayManager dm(display);
  dm.setChoicesRevealed(false);
  TEST_ASSERT_FALSE(dm.isChoicesRevealed());
  TEST_ASSERT_EQUAL(0, dm.getIndicatorHeight()); // 0 when no choices

  dm.setupStoryEndedChoices();
  TEST_ASSERT_TRUE(dm.getIndicatorHeight() > 0);

  dm.revealChoices();
  TEST_ASSERT_TRUE(dm.isChoicesRevealed());

  BootManager::setStoryPath("test/story/story.bin");
  engine.loadStory("test/story/story.bin");
  engine.update();

  TEST_ASSERT_EQUAL(InkEngine::State::STORY_ENDED, engine.getState());
}

void test_choice_activation_skips_blink_when_touch_disabled(void) {
  TestDisplay display;
  MockInput input;
  SDLStorage storage;
  InkEngine engine(display, input, storage);

  AppSettings settings = AppSettings::defaults();
  settings.touchChoicesEnabled = false;

  InkDisplayManager dm(display);
  dm.applySettings(settings);
  dm.setupStoryEndedChoices();

  InkStoryManager story(storage);
  dm.flashChoiceActivation(story, settings, 2);

  // Selected choice is updated, but blinking choice remains unset (-1)
  TEST_ASSERT_EQUAL(2, dm.getSelectedChoice());
  TEST_ASSERT_EQUAL(-1, dm.getBlinkingChoice());
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
  auto runs =
      InkRichTextParser::parse("Line 1<br>Line 2<br/>Line 3<br /><b>Bold</b> "
                               "<i>Italic</i> **MDBold** *MDItalic*");
  std::string fullText = "";
  for (const auto &r : runs) {
    fullText += r.text;
  }
  TEST_ASSERT_EQUAL_STRING(
      "Line 1\nLine 2\nLine 3\nBold Italic MDBold MDItalic", fullText.c_str());
  TEST_ASSERT_TRUE(runs.size() >= 5);
}

void test_builtin_literata_rendering(void) {
  EpdFont fontReg(&literata_medium_2b);
  EpdFont fontBold(&literata_medium_bold_2b);
  EpdFont fontItalic(&literata_medium_italic_2b);

  // Test space codepoint 32
  const EpdGlyph *gSpace = fontReg.getGlyph(32);
  TEST_ASSERT_NOT_NULL(gSpace);
  TEST_ASSERT_TRUE(gSpace->advanceX > 0);

  // Test basic letter
  const EpdGlyph *gA = fontReg.getGlyph('A');
  TEST_ASSERT_NOT_NULL(gA);
  TEST_ASSERT_TRUE(gA->width > 0);
  TEST_ASSERT_TRUE(gA->height > 0);

  // Test dimensions
  int w = 0, h = 0;
  fontReg.getTextDimensions("Hello Literata serif text", &w, &h);
  TEST_ASSERT_TRUE(w > 0);
  TEST_ASSERT_TRUE(h > 0);

  // Test family rendering with GfxRenderer
  TestDisplay display;
  display.clear();
  EpdFontFamily litFamily(&fontReg, &fontBold, &fontItalic);
  display.renderer.insertFont(70, litFamily);
  display.renderer.drawText(
      70, 20, 100,
      "Literata Serif: The quick brown fox jumps over the lazy dog");
  display.renderer.drawText(70, 20, 150, "Literata Italic text", true,
                            EpdFontFamily::ITALIC);
  display.renderer.drawText(70, 20, 200, "Literata Bold text", true,
                            EpdFontFamily::BOLD);
}

void test_save_manager_enk2_serialization(void) {
  SDLStorage storage;
  StorySaveManager mgr;
  const char *testSavePath = "test/test_save.sav";
  mgr.init(testSavePath, 0x12345678);

  uint8_t snapMain[] = {1, 2, 3, 4, 5};
  std::deque<WrappedLine> histMain;
  histMain.push_back({TextBlock("Line 1"), false, false, "", 0, true});
  mgr.saveMainProgress(snapMain, sizeof(snapMain), histMain);

  uint8_t snapCp1[] = {10, 20};
  std::deque<WrappedLine> histCp1;
  histCp1.push_back({TextBlock("Cp 1"), false, false, "", 0, true});
  mgr.saveCheckpoint("", snapCp1, sizeof(snapCp1), histCp1);

  uint8_t snapCh1[] = {30, 40};
  std::deque<WrappedLine> histCh1;
  histCh1.push_back({TextBlock("Ch 1"), false, false, "", 0, true});
  mgr.saveCheckpoint("The Plan", snapCh1, sizeof(snapCh1), histCh1);

  TEST_ASSERT_TRUE(mgr.writeSaveFile(storage));

  StorySaveManager mgr2;
  mgr2.init(testSavePath, 0x12345678);
  TEST_ASSERT_TRUE(mgr2.loadSaveFile(storage));
  TEST_ASSERT_TRUE(mgr2.hasMainProgress());
  TEST_ASSERT_EQUAL(5, mgr2.getMainSnapshot().size());
  TEST_ASSERT_EQUAL(1, mgr2.getMainHistory().size());

  TEST_ASSERT_TRUE(mgr2.hasUnnamedCheckpoint());
  TEST_ASSERT_TRUE(mgr2.hasNamedCheckpoints());
  TEST_ASSERT_EQUAL(2, mgr2.getCheckpoints().size());

  storage.deleteFile(testSavePath);
}

void test_save_manager_universal_key_deduplication(void) {
  StorySaveManager mgr;
  mgr.init("test/dummy.sav", 0x11223344);

  uint8_t dummySnap[] = {1, 2, 3};
  std::deque<WrappedLine> dummyHist;

  // Save unnamed 1
  mgr.saveCheckpoint("", dummySnap, sizeof(dummySnap), dummyHist);
  TEST_ASSERT_EQUAL(1, mgr.getCheckpoints().size());
  TEST_ASSERT_EQUAL(0, mgr.getUnnamedCheckpointIndex());

  // Save named "Chapter 1"
  mgr.saveCheckpoint("Chapter 1", dummySnap, sizeof(dummySnap), dummyHist);
  TEST_ASSERT_EQUAL(2, mgr.getCheckpoints().size());
  TEST_ASSERT_EQUAL(0, mgr.getUnnamedCheckpointIndex());

  // Save unnamed 2 -> should prune older unnamed and append at tail (index 1)
  mgr.saveCheckpoint("", dummySnap, sizeof(dummySnap), dummyHist);
  TEST_ASSERT_EQUAL(2, mgr.getCheckpoints().size());
  TEST_ASSERT_EQUAL_STRING("Chapter 1", mgr.getCheckpoints()[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("", mgr.getCheckpoints()[1].title.c_str());
  TEST_ASSERT_EQUAL(1, mgr.getUnnamedCheckpointIndex());

  // Save named "Chapter 1" again -> should prune index 0 and append at tail
  // (index 1)
  mgr.saveCheckpoint("Chapter 1", dummySnap, sizeof(dummySnap), dummyHist);
  TEST_ASSERT_EQUAL(2, mgr.getCheckpoints().size());
  TEST_ASSERT_EQUAL_STRING("", mgr.getCheckpoints()[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Chapter 1", mgr.getCheckpoints()[1].title.c_str());
}

void test_save_manager_restart_clear(void) {
  SDLStorage storage;
  StorySaveManager mgr;
  const char *testSavePath = "test/test_clear.sav";
  mgr.init(testSavePath, 0x12345678);

  uint8_t snap[] = {1};
  std::deque<WrappedLine> hist;
  mgr.saveMainProgress(snap, sizeof(snap), hist);
  mgr.saveCheckpoint("Ch1", snap, sizeof(snap), hist);
  mgr.writeSaveFile(storage);

  TEST_ASSERT_TRUE(storage.fileExists(testSavePath));
  mgr.clearAll(storage);

  TEST_ASSERT_FALSE(mgr.hasMainProgress());
  TEST_ASSERT_EQUAL(0, mgr.getCheckpoints().size());
  TEST_ASSERT_FALSE(storage.fileExists(testSavePath));
}

void test_story_menu_full_screenshot(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {"Save and exit",
                                    "Rewind to last checkpoint", "Rewind to...",
                                    "Restart story"};

  MenuModalWidget::show(display, input, &widget, "Story Menu", items, 2,
                        "eenk");

  saveBMP("test/golden/test_story_menu_full.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_story_menu_no_checkpoints_screenshot(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {"Save and exit", "Restart story"};

  MenuModalWidget::show(display, input, &widget, "Story Menu", items, 0,
                        "eenk");

  saveBMP("test/golden/test_story_menu_no_checkpoints.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_rewind_to_submenu_screenshot(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {
      "The Plan", "The Heist", "The Twist",
      "The Second Twist Where It Turns Out It Was All Part Of The Plan In The "
      "First Place"};

  MenuModalWidget::show(display, input, &widget, "Rewind to...", items, 3,
                        "eenk");

  saveBMP("test/golden/test_rewind_to_submenu.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_confirm_restart_modal_screenshot(void) {
  TestDisplay display;
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ModalDialogWidget::show(
      display, input, &widget, "Confirm Restart",
      "Are you sure you\nwant to restart ?\nProgress will be lost", "eenk");

  saveBMP("test/golden/test_confirm_restart_modal.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_battery_widget_x3_screenshot(void) {
  TestDisplay display(528, 792);
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.fillRect(0, 0, display.getWidth(), 40, true);
  widget.draw(display.getWidth() - widget.getWidth() - 8,
              (40 - widget.getHeight()) / 2, true);
  widget.draw(display.getWidth() - widget.getWidth() - 8,
              (40 - widget.getHeight()) / 2 + 40, false);
  saveBMP("test/golden/x3/test_battery_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_library_x3_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  TestLibrary library(display, input, widget, nullptr /*frontlight*/, settings);
  library.run();

  saveBMP("test/golden/x3/test_library.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_library_with_epub_x3(void) {
  ensure_test_story_media();

  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  {
    BookEngine engine(display, input);
    engine.applySettings(settings);
    engine.loadBook("test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub");
    engine.update(); // Generates thumb.bin
  }

  TestLibraryWithEpub library(display, input, widget, nullptr /*frontlight*/,
                              settings);
  library.run();

  saveBMP("test/golden/x3/test_library_with_epub.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_settings_view_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  SettingsView view(display, input, widget, nullptr /*frontlight*/, settings);
  view.run();

  saveBMP("test/golden/x3/test_settings_view.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_fonts_x3_screenshot(void) {
  TestDisplay display(528, 792);
  display.clear();

  int y = 40;

  display.renderer.drawText(10, 20, y, "UI Normal 12: The quick brown fox");
  y += display.renderer.getLineHeight(10) + 8;

  display.renderer.drawText(11, 20, y, "UI Bold 12: jumps over the lazy dog");
  y += display.renderer.getLineHeight(11) + 8;

  display.renderer.drawText(
      20, 20, y, "UI Small 10: Sphynx of black quartz, judge my vow");
  y += display.renderer.getLineHeight(20) + 16;

  // Sans Medium
  display.renderer.drawText(50, 20, y,
                            "Sans M Regular: The quick brown fox jumps", true,
                            EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(50) + 6;

  display.renderer.drawText(50, 20, y,
                            "Sans M Italic: The quick brown fox jumps", true,
                            EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(50) + 6;

  display.renderer.drawText(50, 20, y, "Sans M Bold: The quick brown fox jumps",
                            true, EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(50) + 16;

  // Literata Serif Medium
  display.renderer.drawText(70, 20, y,
                            "Literata M Regular: The quick brown fox jumps",
                            true, EpdFontFamily::REGULAR);
  y += display.renderer.getLineHeight(70) + 6;

  display.renderer.drawText(70, 20, y,
                            "Literata M Italic: The quick brown fox jumps",
                            true, EpdFontFamily::ITALIC);
  y += display.renderer.getLineHeight(70) + 6;

  display.renderer.drawText(70, 20, y,
                            "Literata M Bold: The quick brown fox jumps", true,
                            EpdFontFamily::BOLD);
  y += display.renderer.getLineHeight(70) + 16;

  int maxW = display.getWidth() - 40;

  // Rich HTML & MD with Literata
  auto runs1 = InkRichTextParser::parse(
      "Literata HTML: <i>Italic</i>, <b>Bold</b>, <b><i>Both</i></b>!");
  auto blocks1 = display.renderer.wrapRichText(70, runs1, maxW, 10);
  for (const auto &b : blocks1) {
    display.renderer.drawRichText(70, 20, y, b);
    y += display.renderer.getLineHeight(70);
  }
  y += 6;

  auto runs2 =
      InkRichTextParser::parse("Literata MD: *Italic*, **Bold**, **_Both_**!");
  auto blocks2 = display.renderer.wrapRichText(70, runs2, maxW, 10);
  for (const auto &b : blocks2) {
    display.renderer.drawRichText(70, 20, y, b);
    y += display.renderer.getLineHeight(70);
  }
  y += 10;

  // Choice rich text (UI 12)
  auto runs3 =
      InkRichTextParser::parse("Choice UI: *Italic*, **Bold**, **_Both_**!");
  auto blocks3 = display.renderer.wrapRichText(10, runs3, maxW, 10);
  for (const auto &b : blocks3) {
    display.renderer.drawRichText(10, 20, y, b);
    y += display.renderer.getLineHeight(10);
  }

  saveBMP("test/golden/x3/test_fonts.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_external_fonts_x3_screenshot(void) {
  TestDisplay display(528, 792);
  display.clear();

  StreamingEpdFontFamily fam;
  const char *dirs[] = {"test/fonts", "sd_fonts", nullptr};
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

  saveBMP("test/golden/x3/test_external_fonts.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_modal_dialog_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  SystemUI ui(display);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ui.showConfirmDialog(input, "Confirm Action",
                       "Are you sure you want to exit the current story?");

  saveBMP("test/golden/x3/test_modal_dialog.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_modal_dialog_long_text_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  SystemUI ui(display);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ui.showConfirmDialog(
      input, nullptr,
      "Formatting the SD card will erase all saved story data, custom fonts, "
      "and user preferences. This action cannot be undone.");

  saveBMP("test/golden/x3/test_modal_dialog_long.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_loading_widget_x3_screenshot(void) {
  TestDisplay display(528, 792);
  SystemUI ui(display);

  ui.showLoading("Loading Story...", 0.65f);

  saveBMP("test/golden/x3/test_loading_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_error_widget_x3_screenshot(void) {
  TestDisplay display(528, 792);
  SystemUI ui(display);

  ui.showMessage(
      "Failed to load story",
      "The story file is corrupted or could not be found.\nPlease try "
      "re-flashing the device or formatting the SD card.");

  saveBMP("test/golden/x3/test_error_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_sleep_cover_x3_screenshot(void) {
  TestDisplay display(528, 792);
  SystemUI ui(display);

  ui.showSleepCover("Zzzzz...", "Sleep mode");

  saveBMP("test/golden/x3/test_sleep_cover.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_sleep_cover_image_x3_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display(528, 792);
  SystemUI ui(display);

  BootManager::setStoryPath("test/story/story.bin");

  ui.showSleepCover("Zzzzz...", "Sleep mode");

  saveBMP("test/golden/x3/test_sleep_cover_image.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

void test_story_player_x3_screenshot(void) {
  TestDisplay display(528, 792);
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
  int iconSize = 10;
  display.renderer.drawText(choiceFont, 40, y, "1. Look around", true);
  y += display.renderer.getLineHeight(choiceFont) + 10;
  display.renderer.fillRect(
      20, y - 2, display.getWidth() - 40, display.renderer.getLineHeight(choiceFont) + 4, true);
  display.renderer.drawTriangleIcon(
      24, y + (display.renderer.getLineHeight(choiceFont) - iconSize) / 2,
      iconSize, false);
  display.renderer.drawText(choiceFont, 40, y, "2. Go back to sleep", false);
  y += display.renderer.getLineHeight(choiceFont) + 10;
  display.renderer.drawText(choiceFont, 40, y, "3. Yell for help", true);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Power"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/x3/test_story_player.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_page_x3_render(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // Call update to render the first page
  engine.update();

  saveBMP("test/golden/x3/test_epub_page_render.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_page_2_x3_render(void) {
  TestDisplay display(528, 792);
  QueueMockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // First page render (cover)
  engine.update();

  // Advance to page 2 (text page)
  input.push(ButtonEvent::DOWN);
  engine.update();

  saveBMP("test/golden/x3/test_epub_page_2_render.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_epub_exit_modal_x3_render(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  AppSettings settings = AppSettings::defaults();

  BookEngine engine(display, input);
  engine.setBatteryWidget(&widget);
  engine.applySettings(settings);

  const char *epubPath = "test/the-strange-case-of-dr-jekyll-and-mr-hyde.epub";
  bool loaded = engine.loadBook(epubPath);
  TEST_ASSERT_TRUE_MESSAGE(loaded, "Failed to load test EPUB");

  // First page render (cover)
  engine.update();

  // Show exit modal with progress percentage and battery widget
  char progressStr[32];
  snprintf(progressStr, sizeof(progressStr), "%d%%",
           engine.getProgressPercentage());
  ModalDialogWidget::show(display, input, &widget, "Exit Reader",
                          "Do you want to exit to the menu?", progressStr);

  saveBMP("test/golden/x3/test_epub_exit_modal.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_choice_states_x3_screenshot(void) {
  TestDisplay display(528, 792);
  display.clear();

  int marginX = 24;
  int width = display.getWidth();
  int narrativeWidth = width - (2 * marginX);
  int indicatorWidth = 24;

  int y = 45;
  int narrativeFont = 50; // Sans Medium

  auto narrativeRuns = InkRichTextParser::parse(
      "You stand at the threshold of the forgotten vault. A cold whisper "
      "echoes from the darkness below, asking what path you will choose.");
  auto narrativeBlocks = display.renderer.wrapRichText(
      narrativeFont, narrativeRuns, narrativeWidth, 10);
  for (const auto &b : narrativeBlocks) {
    display.renderer.drawRichText(narrativeFont, marginX, y, b);
    y += display.renderer.getLineHeight(narrativeFont);
  }
  y += 15;

  display.renderer.fillRect(marginX, y, narrativeWidth, 2, true);
  y += 20;

  int choiceFont = 10; // UI 12
  int choiceLineHeight = display.renderer.getLineHeight(choiceFont);
  int choicePadding = choiceLineHeight / 3;
  int choiceContentWidth = narrativeWidth - indicatorWidth;

  auto drawChoiceItem = [&](const char *text, bool focused) {
    auto runs = InkRichTextParser::parse(text);
    auto blocks =
        display.renderer.wrapRichText(choiceFont, runs, choiceContentWidth, 10);
    int numLines = std::max((size_t)1, blocks.size());
    int textHeight = numLines * choiceLineHeight;
    int blockHeight = textHeight + 8;
    int verticalOffset = 4;

    if (focused) {
      display.renderer.fillRect(marginX - 4, y, narrativeWidth + 8, blockHeight,
                                true);
    }

    bool textBlack = !focused;
    int iconSize = 10;
    int iconX = marginX + 4;
    int textY = y + verticalOffset;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (i == 0 && focused) {
        display.renderer.drawTriangleIcon(
            iconX, textY + (choiceLineHeight - iconSize) / 2, iconSize,
            textBlack);
      }
      display.renderer.drawRichText(choiceFont, marginX + indicatorWidth, textY,
                                    blocks[i], textBlack);
      textY += choiceLineHeight;
    }
    y += blockHeight + choicePadding;
  };

  drawChoiceItem("Examine the runes on the archway", false);
  drawChoiceItem("Step forward with your **drawn blade**", false);
  drawChoiceItem("Turn back and <i>seal the heavy gate</i>", true);
  drawChoiceItem("Search the stone walls for hidden mechanisms", false);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Back"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/x3/test_choices.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_choice_states_touch_x3_screenshot(void) {
  TestDisplay display(528, 792);
  display.clear();

  int marginX = 24;
  int width = display.getWidth();
  int narrativeWidth = width - (2 * marginX);
  int indicatorWidth = 24;

  int y = 45;
  int narrativeFont = 50; // Sans Medium

  auto narrativeRuns = InkRichTextParser::parse(
      "You stand at the threshold of the forgotten vault. A cold whisper "
      "echoes from the darkness below, asking what path you will choose.");
  auto narrativeBlocks = display.renderer.wrapRichText(
      narrativeFont, narrativeRuns, narrativeWidth, 10);
  for (const auto &b : narrativeBlocks) {
    display.renderer.drawRichText(narrativeFont, marginX, y, b);
    y += display.renderer.getLineHeight(narrativeFont);
  }
  y += 15;

  display.renderer.fillRect(marginX, y, narrativeWidth, 2, true);
  y += 20;

  int choiceFont = 10; // UI 12
  int choiceLineHeight = display.renderer.getLineHeight(choiceFont);
  int choicePadding = choiceLineHeight / 3;
  int choiceContentWidth = narrativeWidth - indicatorWidth;

  auto drawChoiceItem = [&](const char *text, bool focused) {
    auto runs = InkRichTextParser::parse(text);
    auto blocks =
        display.renderer.wrapRichText(choiceFont, runs, choiceContentWidth, 10);
    int numLines = std::max((size_t)1, blocks.size());
    int textHeight = numLines * choiceLineHeight;
    int blockHeight = std::max(64, textHeight);
    int verticalOffset = (blockHeight - textHeight) / 2;

    if (focused) {
      display.renderer.fillRect(marginX - 4, y, narrativeWidth + 8, blockHeight,
                                true);
    }

    bool textBlack = !focused;
    int iconSize = 10;
    int iconX = marginX + 4;
    int textY = y + verticalOffset;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (i == 0 && focused) {
        display.renderer.drawTriangleIcon(
            iconX, textY + (choiceLineHeight - iconSize) / 2, iconSize,
            textBlack);
      }
      display.renderer.drawRichText(choiceFont, marginX + indicatorWidth, textY,
                                    blocks[i], textBlack);
      textY += choiceLineHeight;
    }
    y += blockHeight + choicePadding;
  };

  drawChoiceItem("Examine the runes on the archway", true);
  drawChoiceItem("Step forward with your **drawn blade**", false);
  drawChoiceItem("Turn back and <i>seal the heavy gate</i>", false);

  FooterWidget footer;
  footer.btnBack = {true, "Menu", "Back"};
  footer.btnConfirm = {true, "Select", "Confirm"};
  footer.btnPrev = {true, "Up", "Prev"};
  footer.btnNext = {true, "Down", "Next"};
  footer.render(&display.renderer, display.getWidth(), display.getHeight());

  saveBMP("test/golden/x3/test_choices_touch.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_quick_menu_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  AppSettings settings = AppSettings::defaults();
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  QuickMenuWidget ui(display, input, widget, nullptr /*frontlight*/, settings);

  display.clear();
  display.renderer.drawText(10, 50, 600, "Background text...", true);

  ui.show();

  saveBMP("test/golden/x3/test_quick_menu.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

void test_quick_menu_disabled_choices_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  AppSettings settings = AppSettings::defaults();
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);
  QuickMenuWidget ui(display, input, widget, nullptr /*frontlight*/, settings,
                     /*choicesEnabled=*/false);

  display.clear();
  display.renderer.drawText(10, 50, 600, "Background text...", true);

  ui.show();

  saveBMP("test/golden/x3/test_quick_menu_book_mode.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

void test_image_widget_x3_screenshot(void) {
  ensure_test_story_media();

  TestDisplay display(528, 792);
  MockInput input;
  SDLStorage storage;
  InkEngine engine(display, input, storage);

  BootManager::setStoryPath("test/story/story.bin");
  engine.loadStory("test/story/story.bin");
  engine.update();

  saveBMP("test/golden/x3/test_image_widget.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());

  TEST_ASSERT_EQUAL(1, 1);
}

void test_story_menu_full_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {"Save and exit",
                                    "Rewind to last checkpoint", "Rewind to...",
                                    "Restart story"};

  MenuModalWidget::show(display, input, &widget, "Story Menu", items, 2,
                        "eenk");

  saveBMP("test/golden/x3/test_story_menu_full.bmp", display.eink.getFrameBuffer(),
          display.getWidth(), display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_story_menu_no_checkpoints_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {"Save and exit", "Restart story"};

  MenuModalWidget::show(display, input, &widget, "Story Menu", items, 0,
                        "eenk");

  saveBMP("test/golden/x3/test_story_menu_no_checkpoints.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_rewind_to_submenu_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  std::vector<std::string> items = {
      "The Plan", "The Heist", "The Twist",
      "The Second Twist Where It Turns Out It Was All Part Of The Plan In The "
      "First Place"};

  MenuModalWidget::show(display, input, &widget, "Rewind to...", items, 3,
                        "eenk");

  saveBMP("test/golden/x3/test_rewind_to_submenu.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}

void test_confirm_restart_modal_x3_screenshot(void) {
  TestDisplay display(528, 792);
  MockInput input;
  BatteryMonitor battery;
  BatteryWidget widget(display.renderer, battery);

  display.clear();
  display.renderer.drawText(10, 50, 100, "Background text...");

  ModalDialogWidget::show(
      display, input, &widget, "Confirm Restart",
      "Are you sure you\nwant to restart ?\nProgress will be lost", "eenk");

  saveBMP("test/golden/x3/test_confirm_restart_modal.bmp",
          display.eink.getFrameBuffer(), display.getWidth(),
          display.getHeight());
  TEST_ASSERT_EQUAL(1, 1);
}
#endif

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_script_detector);
#ifdef PLATFORM_NATIVE
  RUN_TEST(test_sd_font_catalogue);
  RUN_TEST(test_rich_text_parser);
  RUN_TEST(test_builtin_literata_rendering);
  RUN_TEST(test_battery_widget_screenshot);
  RUN_TEST(test_library_screenshot);
  RUN_TEST(test_library_with_epub);
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
  RUN_TEST(test_epub_page_render);
  RUN_TEST(test_epub_page_2_render);
  RUN_TEST(test_epub_exit_modal_render);
  RUN_TEST(test_epub_bookmark_resume);
  RUN_TEST(test_epub_framebuffer_cache);
  RUN_TEST(test_epub_touch_disabled_navigation);
  RUN_TEST(test_choice_states_screenshot);
  RUN_TEST(test_choice_states_touch_screenshot);
  RUN_TEST(test_image_widget_screenshot);
  RUN_TEST(test_choice_reveal_interaction);
  RUN_TEST(test_choice_activation_skips_blink_when_touch_disabled);
  RUN_TEST(test_story_metadata_save_path);
  RUN_TEST(test_quick_menu_screenshot);
  RUN_TEST(test_quick_menu_disabled_choices_screenshot);
  // StreamingEpdFontFamily unit tests
  RUN_TEST(test_streaming_epd_font_family_load_plain);
  RUN_TEST(test_streaming_epd_font_family_load_regular_suffix);
  RUN_TEST(test_streaming_epd_font_family_fallback_chain);
  RUN_TEST(test_streaming_epd_font_family_all_styles);
  RUN_TEST(test_streaming_epd_font_family_missing_regular_fails);
  RUN_TEST(test_streaming_epd_font_family_bold_italic_fallback_order);
  RUN_TEST(test_sd_font_catalogue_family_detection);
  // Save manager & Menu modal tests
  RUN_TEST(test_save_manager_enk2_serialization);
  RUN_TEST(test_save_manager_universal_key_deduplication);
  RUN_TEST(test_save_manager_restart_clear);
  RUN_TEST(test_story_menu_full_screenshot);
  RUN_TEST(test_story_menu_no_checkpoints_screenshot);
  RUN_TEST(test_rewind_to_submenu_screenshot);
  RUN_TEST(test_confirm_restart_modal_screenshot);

  // X3 (792x528) Golden Screenshot Tests
  RUN_TEST(test_battery_widget_x3_screenshot);
  RUN_TEST(test_library_x3_screenshot);
  RUN_TEST(test_library_with_epub_x3);
  RUN_TEST(test_settings_view_x3_screenshot);
  RUN_TEST(test_fonts_x3_screenshot);
  RUN_TEST(test_external_fonts_x3_screenshot);
  RUN_TEST(test_modal_dialog_x3_screenshot);
  RUN_TEST(test_modal_dialog_long_text_x3_screenshot);
  RUN_TEST(test_loading_widget_x3_screenshot);
  RUN_TEST(test_error_widget_x3_screenshot);
  RUN_TEST(test_sleep_cover_x3_screenshot);
  RUN_TEST(test_sleep_cover_image_x3_screenshot);
  RUN_TEST(test_story_player_x3_screenshot);
  RUN_TEST(test_epub_page_x3_render);
  RUN_TEST(test_epub_page_2_x3_render);
  RUN_TEST(test_epub_exit_modal_x3_render);
  RUN_TEST(test_choice_states_x3_screenshot);
  RUN_TEST(test_choice_states_touch_x3_screenshot);
  RUN_TEST(test_quick_menu_x3_screenshot);
  RUN_TEST(test_quick_menu_disabled_choices_x3_screenshot);
  RUN_TEST(test_image_widget_x3_screenshot);
  RUN_TEST(test_story_menu_full_x3_screenshot);
  RUN_TEST(test_story_menu_no_checkpoints_x3_screenshot);
  RUN_TEST(test_rewind_to_submenu_x3_screenshot);
  RUN_TEST(test_confirm_restart_modal_x3_screenshot);
#endif
  UNITY_END();
  return 0;
}
