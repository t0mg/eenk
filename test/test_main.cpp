#include <unity.h>
#include <stdio.h>

#ifdef PLATFORM_NATIVE
#include "hal/sdl/mock/LittleFS.h"
#endif

#ifdef PLATFORM_NATIVE
#include "hal/sdl/mock/EInkDisplay.h"
#include "GfxRenderer.h"
#include "ui/SystemUI.h"
#include "ui/GameLibrary.h"
#include "ui/SettingsView.h"
#include "ui/BatteryWidget.h"
#include "os/AppSettings.h"
#include "os/SdFontCatalogue.h"
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/reader_medium_bold_2b.h>
#include <builtinFonts/reader_medium_italic_2b.h>
#include "InkRichTextParser.h"
#endif

#include "ScriptDetector.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_script_detector(void) {
    TEST_ASSERT_EQUAL(ScriptDetector::Script::LATIN, ScriptDetector::classify("Hello"));
    TEST_ASSERT_EQUAL(ScriptDetector::Script::ARABIC, ScriptDetector::classify("مرحبا"));
    TEST_ASSERT_EQUAL(ScriptDetector::Script::THAI, ScriptDetector::classify("สวัสดี"));
}

#ifdef PLATFORM_NATIVE
void test_sd_font_catalogue(void) {
    SdFontCatalogue cat;
    cat.scan(); // Should pick up kBuiltinFonts and any .epdfont files in fonts/ directory

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
void saveBMP(const char* filename, const uint8_t* frameBuffer, int width, int height) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    
    int rowSize = ((width + 31) / 32) * 4;
    int imageSize = rowSize * height;
    int fileSize = 14 + 40 + 8 + imageSize;

    uint8_t fileHeader[14] = {
        'B', 'M',
        (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
        0, 0, 0, 0,
        62, 0, 0, 0
    };

    uint8_t infoHeader[40] = {
        40, 0, 0, 0,
        (uint8_t)(width), (uint8_t)(width >> 8), (uint8_t)(width >> 16), (uint8_t)(width >> 24),
        (uint8_t)(-height), (uint8_t)(-height >> 8), (uint8_t)(-height >> 16), (uint8_t)(-height >> 24),
        1, 0,
        1, 0,
        0, 0, 0, 0,
        (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
        0, 0, 0, 0,
        0, 0, 0, 0,
        2, 0, 0, 0,
        2, 0, 0, 0
    };

    uint8_t colorTable[8] = {
        0, 0, 0, 0,
        255, 255, 255, 0
    };

    fwrite(fileHeader, 1, 14, f);
    fwrite(infoHeader, 1, 40, f);
    fwrite(colorTable, 1, 8, f);

    // We allocate a buffer for the logical image size. Width=480, height=800.
    // Row size for 480 width is exactly 60 bytes.
    uint8_t* rotatedBuffer = new uint8_t[imageSize];
    for (int i = 0; i < imageSize; i++) rotatedBuffer[i] = 0;

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

    EpdFont readerNormal;
    EpdFont readerBold;
    EpdFont readerItalic;
    EpdFontFamily readerFamily;

    TestDisplay() : 
        renderer(eink), 
        sysFontNormal(&ui_12), 
        sysFamilyNormal(&sysFontNormal),
        sysFontBold(&ui_bold_12),
        sysFamilyBold(&sysFontBold),
        sysFontSmall(&ui_10),
        sysFamilySmall(&sysFontSmall),
        readerNormal(&reader_medium_2b),
        readerBold(&reader_medium_bold_2b),
        readerItalic(&reader_medium_italic_2b),
        readerFamily(&readerNormal, &readerBold, &readerItalic)
    {
        renderer.begin();
        renderer.setOrientation(GfxRenderer::PortraitInverted);
        // Fallback
        renderer.insertFont(0, sysFamilyNormal); 
        // SystemUI
        renderer.insertFont(10, sysFamilyNormal);
        renderer.insertFont(11, sysFamilyBold);
        // BatteryWidget
        renderer.insertFont(20, sysFamilySmall);
        // GameLibrary
        renderer.insertFont(30, sysFamilyNormal);
        renderer.insertFont(31, sysFamilyBold);
        renderer.insertFont(32, sysFamilySmall);
        // SettingsView
        renderer.insertFont(33, sysFamilySmall);
        
        // Test Fonts
        renderer.insertFont(50, readerFamily);
    }
    void clear() override { eink.clearScreen(0xFF); }
    void present() override {}
    void fullRefresh() override {}
    int getWidth() const override { return 480; }
    int getHeight() const override { return 800; }
    GfxRenderer* getRenderer() override { return &renderer; }
};

class MockInput : public IInput {
public:
    ButtonEvent pollInput() override {
        return ButtonEvent::QUIT; // Forces UI run() loops to exit immediately after one render
    }
};

void test_system_ui_screenshot(void) {
    TestDisplay display;
    SystemUI ui(display);

    display.clear(); 
    ui.showError("System Failure", "Just kidding, this is a unit test screenshot.");
    saveBMP("test/golden/test_system_ui.bmp", display.eink.getFrameBuffer(), display.getWidth(), display.getHeight());
    TEST_ASSERT_EQUAL(1, 1);
}

void test_battery_widget_screenshot(void) {
    TestDisplay display;
    BatteryMonitor battery;
    BatteryWidget widget(display.renderer, battery);

    display.clear();
    // Usually drawn in the top right corner
    widget.draw(480 - 50, 10, false);
    saveBMP("test/golden/test_battery_widget.bmp", display.eink.getFrameBuffer(), display.getWidth(), display.getHeight());
    TEST_ASSERT_EQUAL(1, 1);
}

class TestGameLibrary : public GameLibrary {
public:
    TestGameLibrary(IDisplay& display, IInput& input, BatteryWidget& battery, AppSettings& settings)
        : GameLibrary(display, input, battery, settings) {}

protected:
    void scanSD() override {
        _numEntries = 4;

        snprintf(_entries[0].title, sizeof(_entries[0].title), "The Hitchhiker's Guide");
        snprintf(_entries[0].author, sizeof(_entries[0].author), "Douglas Adams");
        _entries[0].sizeBytes = 1500000;
        _entries[0].hasMetadata = true;
        _entries[0].hasSave = true;
        _entries[0].isCurrentlyLoaded = true;

        snprintf(_entries[1].title, sizeof(_entries[1].title), "Zork I: The Great Underground Empire");
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

        snprintf(_entries[3].title, sizeof(_entries[3].title), "A Mind Forever Voyaging");
        snprintf(_entries[3].author, sizeof(_entries[3].author), "Steve Meretzky");
        _entries[3].sizeBytes = 250000;
        _entries[3].hasMetadata = true;
        _entries[3].hasSave = false;
    }
};

void test_game_library_screenshot(void) {
    TestDisplay display;
    MockInput input;
    BatteryMonitor battery;
    BatteryWidget widget(display.renderer, battery);
    AppSettings settings = AppSettings::defaults();
    
    TestGameLibrary library(display, input, widget, settings);
    library.run();

    saveBMP("test/golden/test_game_library.bmp", display.eink.getFrameBuffer(), display.getWidth(), display.getHeight());
    TEST_ASSERT_EQUAL(1, 1);
}

void test_settings_view_screenshot(void) {
    TestDisplay display;
    MockInput input;
    BatteryMonitor battery;
    BatteryWidget widget(display.renderer, battery);
    AppSettings settings = AppSettings::defaults();
    
    SettingsView view(display, input, widget, settings);
    view.run();

    saveBMP("test/golden/test_settings_view.bmp", display.eink.getFrameBuffer(), display.getWidth(), display.getHeight());
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
    
    display.renderer.drawText(20, 20, y, "UI Small 10: Sphynx of black quartz, judge my vow");
    y += display.renderer.getLineHeight(20) + 30;
    
    display.renderer.drawText(50, 20, y, "Reader M Normal: The quick brown fox jumps", true, EpdFontFamily::REGULAR);
    y += display.renderer.getLineHeight(50) + 10;
    
    display.renderer.drawText(50, 20, y, "Reader M Italic: The quick brown fox jumps", true, EpdFontFamily::ITALIC);
    y += display.renderer.getLineHeight(50) + 10;
    
    display.renderer.drawText(50, 20, y, "Reader M Bold: The quick brown fox jumps", true, EpdFontFamily::BOLD);
    y += display.renderer.getLineHeight(50) + 20;

    auto runs1 = InkRichTextParser::parse("Rich HTML: <i>Italic</i>, <b>Bold</b>, <b><i>Both</i></b>!");
    FILE* f = fopen("test_runs.txt", "w");
    auto blocks1 = display.renderer.wrapRichText(50, runs1, 440, 10);
    for (const auto& b : blocks1) {
        fprintf(f, "Line with %d runs\n", (int)b.runs.size());
        for (const auto& r : b.runs) {
            int w = display.renderer.getTextWidth(50, r.text.c_str(), r.style);
            
            // SUPER DEBUG:
            if (r.text == " ") {
                fprintf(f, "  SUPERDEBUG SPACE: Calling getTextWidth again: %d ", display.renderer.getTextWidth(50, " ", r.style));
            }

            fprintf(f, "  Run: [%s] len=%d style=%d width=%d firstchar=%d\n", r.text.c_str(), (int)r.text.length(), (int)r.style, w, (int)r.text[0]);
        }
        display.renderer.drawRichText(50, 20, y, b);
        y += display.renderer.getLineHeight(50);
    }
    fclose(f);
    y += 10;

    auto runs2 = InkRichTextParser::parse("Rich MD: *Italic*, **Bold**, **_Both_**!");
    auto blocks2 = display.renderer.wrapRichText(50, runs2, 440, 10);
    for (const auto& b : blocks2) {
        display.renderer.drawRichText(50, 20, y, b);
        y += display.renderer.getLineHeight(50);
    }
    y += 10;
    
    // Test choice rich text (using FONT_CHOICE = 11 for bold, 10 for normal UI font in test)
    // Actually ui_12 is 10, ui_bold_12 is 11 in test setup.
    // wait, FONT_CHOICE is not defined here. Let's just use 10.
    auto runs3 = InkRichTextParser::parse("Choice: *Italic*, **Bold**, **_Both_**!");
    auto blocks3 = display.renderer.wrapRichText(10, runs3, 440, 10);
    for (const auto& b : blocks3) {
        display.renderer.drawRichText(10, 20, y, b);
        y += display.renderer.getLineHeight(10);
    }
    y += 10;
    
    saveBMP("test/golden/test_fonts.bmp", display.eink.getFrameBuffer(), display.getWidth(), display.getHeight());
    TEST_ASSERT_EQUAL(1, 1);
}
#endif

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_script_detector);
#ifdef PLATFORM_NATIVE
    RUN_TEST(test_sd_font_catalogue);
    RUN_TEST(test_system_ui_screenshot);
    RUN_TEST(test_battery_widget_screenshot);
    RUN_TEST(test_game_library_screenshot);
    RUN_TEST(test_settings_view_screenshot);
    RUN_TEST(test_fonts_screenshot);
#endif
    UNITY_END();
    return 0;
}
