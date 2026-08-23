#include "BookFontManager.h"
#include "os/AppSettings.h"
#include <BuiltinFonts.h>
#include <text/hyph_en_us.h>
#include <cstdio>
#include <cstdlib>

#ifdef PLATFORM_ESP32
#include <esp_heap_caps.h>
#include <FS.h>
#ifdef USE_SD_MMC
#include <SD_MMC.h>
#define SD_FS SD_MMC
#else
#include <SD.h>
#define SD_FS SD
#endif
#endif

namespace BookFontManager {

FontSetup::FontSetup() = default;

FontSetup::~FontSetup() {
    for (int i = 0; i < epdFontCount; i++) {
        delete epdFonts[i];
    }
    for (int i = 0; i < builtinFontCount; i++) {
        delete builtinFonts[i];
    }
    for (int i = 0; i < streamingFontCount; i++) {
        delete streamingFonts[i];
    }
    if (ttfData) {
        free(ttfData);
    }
}

bool setup(FontSetup& out, freeink::book::Arena& glyphArena, const AppSettings& settings) {
    const char* stem = settings.storyFont;
    if (!stem || stem[0] == '\0') {
        stem = "sans-medium";
    }

    uint8_t targetSize = 16;

    // Fingerprint: basic hash of stem and size
    out.fingerprint = targetSize;
    for (int i = 0; stem[i]; i++) {
        out.fingerprint = (out.fingerprint * 31) + stem[i];
    }

    // 1. Try TTF
#if defined(BOARD_HAS_PSRAM) || defined(PLATFORM_NATIVE)
    char ttfPath[128];
    snprintf(ttfPath, sizeof(ttfPath), "/fonts/%s.ttf", stem);
    
    // Quick file check
#if defined(PLATFORM_NATIVE)
    FILE* f = fopen(ttfPath, "rb");
    if (!f) {
        snprintf(ttfPath, sizeof(ttfPath), "fonts/%s.ttf", stem);
        f = fopen(ttfPath, "rb");
    }
    if (f) {
        fseek(f, 0, SEEK_END);
        out.ttfDataLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        out.ttfData = (uint8_t*)malloc(out.ttfDataLen);
        fread(out.ttfData, 1, out.ttfDataLen, f);
        fclose(f);
        
        if (out.ttfFont.init(out.ttfData, out.ttfDataLen, glyphArena)) {
            out.chain.add(&out.ttfFont, freeink::book::StyleNone);
            out.fingerprint += 1000;
        }
    }
#else
    if (SD_FS.exists(ttfPath)) {
        File f = SD_FS.open(ttfPath, FILE_READ);
        if (f) {
            out.ttfDataLen = f.size();
#ifdef BOARD_HAS_PSRAM
            out.ttfData = (uint8_t*)heap_caps_malloc(out.ttfDataLen, MALLOC_CAP_SPIRAM);
#else
            out.ttfData = (uint8_t*)malloc(out.ttfDataLen);
#endif
            if (out.ttfData) {
                f.read(out.ttfData, out.ttfDataLen);
                if (out.ttfFont.init(out.ttfData, out.ttfDataLen, glyphArena)) {
                    out.chain.add(&out.ttfFont, freeink::book::StyleNone);
                    out.fingerprint += 1000;
                }
            }
            f.close();
        }
    }
#endif
#endif

    // 2. EpdFont fallback
    if (out.chain.styleCoverage() == 0) {
        char epdPath[128];
#if defined(PLATFORM_NATIVE)
        snprintf(epdPath, sizeof(epdPath), "fonts/%s-regular.epdfont", stem);
#else
        snprintf(epdPath, sizeof(epdPath), "/fonts/%s-regular.epdfont", stem);
#endif
        
        StreamingEpdFont* streamFont = new StreamingEpdFont();
        if (streamFont->load(epdPath)) {
            out.streamingFonts[out.streamingFontCount++] = streamFont;
            EpdBookFont* bookFont = new EpdBookFont(*streamFont);
            out.epdFonts[out.epdFontCount++] = bookFont;
            out.chain.add(bookFont, freeink::book::StyleNone);
            out.fingerprint += 2000;
        } else {
            delete streamFont;
            
            // Built-in fallback
            const BuiltinFontEntry* builtin = findBuiltinByToken(stem);
            if (!builtin) {
                builtin = getBuiltinByIndex(kDefaultBuiltinFontIndex);
            }
            
            if (builtin && builtin->regular) {
                EpdFont* ef = new EpdFont(builtin->regular);
                out.builtinFonts[out.builtinFontCount++] = ef;
                EpdBookFont* bf = new EpdBookFont(*ef);
                out.epdFonts[out.epdFontCount++] = bf;
                out.chain.add(bf, freeink::book::StyleNone);
                out.fingerprint += 3000;
            }
            if (builtin && builtin->bold) {
                EpdFont* ef = new EpdFont(builtin->bold);
                out.builtinFonts[out.builtinFontCount++] = ef;
                EpdBookFont* bf = new EpdBookFont(*ef);
                out.epdFonts[out.epdFontCount++] = bf;
                out.chain.add(bf, freeink::book::StyleBold);
            }
            if (builtin && builtin->italic) {
                EpdFont* ef = new EpdFont(builtin->italic);
                out.builtinFonts[out.builtinFontCount++] = ef;
                EpdBookFont* bf = new EpdBookFont(*ef);
                out.epdFonts[out.epdFontCount++] = bf;
                out.chain.add(bf, freeink::book::StyleItalic);
            }
            if (builtin && builtin->boldItalic) {
                EpdFont* ef = new EpdFont(builtin->boldItalic);
                out.builtinFonts[out.builtinFontCount++] = ef;
                EpdBookFont* bf = new EpdBookFont(*ef);
                out.epdFonts[out.epdFontCount++] = bf;
                out.chain.add(bf, freeink::book::StyleBold | freeink::book::StyleItalic);
            }
        }
    }

    // 3. Hyphenator
    out.hyphenator.init(freeink::book::k_hyph_en_us, freeink::book::k_hyph_en_us_size);

    return out.chain.styleCoverage() != 0;
}

} // namespace BookFontManager
