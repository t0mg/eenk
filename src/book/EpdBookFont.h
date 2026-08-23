#pragma once

#include <BookFont.h>
#include <EpdFont.h>
#include <StreamingEpdFont.h>

class EpdBookFont : public freeink::book::RenderFont {
public:
    // Initialize from a loaded EpdFont (builtin or SD-loaded, for platforms with enough RAM)
    explicit EpdBookFont(const EpdFont& font);
    // Initialize from a StreamingEpdFont (SD-streamed with LRU cache, for ESP32-C3)
    explicit EpdBookFont(StreamingEpdFont& font);

    bool hasGlyph(uint32_t codepoint) const override;
    int16_t advance(uint32_t codepoint, uint16_t sizePx, uint8_t style) override;
    int16_t lineHeight(uint16_t sizePx) override;
    int16_t ascent(uint16_t sizePx) override;
    const freeink::book::GlyphBitmap* rasterize(uint32_t codepoint, uint16_t sizePx) override;
    int16_t kerning(uint32_t left, uint32_t right, uint16_t sizePx, uint8_t styleFlags) override;
    uint32_t ligature(uint32_t left, uint32_t right, uint8_t styleFlags) override;

private:
    const EpdFont* _font = nullptr;
    StreamingEpdFont* _streamFont = nullptr;
    freeink::book::GlyphBitmap _glyph{};
    uint8_t _coverage[64 * 64];

    static uint32_t normalized(uint32_t cp);
    const EpdGlyph* getEpdGlyph(uint32_t cp) const;
    const uint8_t* getEpdGlyphBitmap(const EpdGlyph* glyph) const;
};
