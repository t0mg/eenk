#pragma once
#include <BookFont.h>
#include <BookArena.h>
#include "EpdBookFont.h"
#include <render/TtfFont.h>
#include <text/Hyphenator.h>
#include <StreamingEpdFont.h>
#include <EpdFont.h>

struct AppSettings;

namespace BookFontManager {
    // Result struct holding all font-related objects.
    // The caller (BookEngine) owns these and must keep them alive.
    struct FontSetup {
        // The FontChain ready for use with LayoutParams and PageRenderer
        freeink::book::FontChain chain;
        
        // EpdBookFont instances (allocated on heap to support explicit constructors)
        EpdBookFont* epdFonts[4] = {nullptr};
        int epdFontCount = 0;
        
        // EpdFont instances (for builtins)
        EpdFont* builtinFonts[4] = {nullptr};
        int builtinFontCount = 0;

        // StreamingEpdFont instances (owned, for lifetime management)
        StreamingEpdFont* streamingFonts[4] = {nullptr};
        int streamingFontCount = 0;
        
        // TtfFont (PSRAM only, optional)
        freeink::book::TtfFont ttfFont;
        uint8_t* ttfData = nullptr;  // heap/PSRAM allocated, must be freed
        uint32_t ttfDataLen = 0;
        
        // Hyphenator
        freeink::book::Hyphenator hyphenator;
        
        // Font fingerprint for layout cache keying
        uint32_t fingerprint = 0;
        
        FontSetup();
        ~FontSetup();  // cleans up heap allocations
    };

    // Builds the font chain from available resources.
    // Priority: TtfFont (PSRAM only) → EpdBookFont (all platforms)
    // Returns true on success (at least one font registered).
    bool setup(FontSetup& out, freeink::book::Arena& glyphArena,
              const AppSettings& settings);
}
