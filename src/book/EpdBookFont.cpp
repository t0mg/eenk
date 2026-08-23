#include "EpdBookFont.h"

EpdBookFont::EpdBookFont(const EpdFont& font) : _font(&font), _streamFont(nullptr) {}

EpdBookFont::EpdBookFont(StreamingEpdFont& font) : _font(nullptr), _streamFont(&font) {}

uint32_t EpdBookFont::normalized(uint32_t cp) {
    switch (cp) {
        case 0x2018: case 0x2019: case 0x02BC: return 0x27;  // '
        case 0x201C: case 0x201D: return 0x22;               // "
        case 0x2013: case 0x2014: case 0x2212: return 0x2D;  // -
        case 0x2026: return 0x2E;                            // .
        case 0x00A0: return 0x20;                            // nbsp
        default: return cp;
    }
}

const EpdGlyph* EpdBookFont::getEpdGlyph(uint32_t cp) const {
    auto lookup = [this](uint32_t codepoint) -> const EpdGlyph* {
        if (_font) return _font->getGlyph(codepoint);
        if (_streamFont) return const_cast<StreamingEpdFont*>(_streamFont)->getGlyph(codepoint);
        return nullptr;
    };
    
    const EpdGlyph* g = lookup(cp);
    if (g == nullptr) {
        uint32_t norm = normalized(cp);
        if (norm != cp) {
            g = lookup(norm);
        }
    }
    return g;
}

const uint8_t* EpdBookFont::getEpdGlyphBitmap(const EpdGlyph* glyph) const {
    if (_font && _font->data) {
        return _font->data->bitmap + glyph->dataOffset;
    } else if (_streamFont) {
        return const_cast<StreamingEpdFont*>(_streamFont)->getGlyphBitmap(glyph);
    }
    return nullptr;
}

bool EpdBookFont::hasGlyph(uint32_t codepoint) const {
    return getEpdGlyph(codepoint) != nullptr;
}

int16_t EpdBookFont::advance(uint32_t codepoint, uint16_t sizePx, uint8_t style) {
    const EpdGlyph* g = getEpdGlyph(codepoint);
    if (g != nullptr) return g->advanceX;
    
    const EpdGlyph* space = getEpdGlyph(' ');
    if (space != nullptr) return space->advanceX;
    
    return 0;
}

int16_t EpdBookFont::lineHeight(uint16_t sizePx) {
    if (_font && _font->data) return _font->data->advanceY;
    if (_streamFont) return _streamFont->getAdvanceY();
    return 0;
}

int16_t EpdBookFont::ascent(uint16_t sizePx) {
    if (_font && _font->data) return _font->data->ascender;
    if (_streamFont) return _streamFont->getAscender();
    return 0;
}

int16_t EpdBookFont::kerning(uint32_t left, uint32_t right, uint16_t sizePx, uint8_t styleFlags) {
    return 0;
}

uint32_t EpdBookFont::ligature(uint32_t left, uint32_t right, uint8_t styleFlags) {
    return 0;
}

const freeink::book::GlyphBitmap* EpdBookFont::rasterize(uint32_t codepoint, uint16_t sizePx) {
    const EpdGlyph* g = getEpdGlyph(codepoint);
    if (g == nullptr) return nullptr;

    const uint8_t* src = getEpdGlyphBitmap(g);
    if (src == nullptr) return nullptr;

    uint32_t pixels = static_cast<uint32_t>(g->width) * g->height;
    if (pixels > sizeof(_coverage)) return nullptr;

    bool is2Bit = false;
    if (_font && _font->data) {
        is2Bit = _font->data->is2Bit;
    } else if (_streamFont) {
        is2Bit = _streamFont->is2Bit();
    }

    if (is2Bit) {
        for (uint32_t i = 0; i < pixels; ++i) {
            const uint8_t byte = src[i / 4];
            const uint8_t bitIdx = static_cast<uint8_t>((3 - (i % 4)) * 2);
            const uint8_t rawVal = (byte >> bitIdx) & 0x3;
            _coverage[i] = rawVal * 85;
        }
    } else {
        for (uint32_t i = 0; i < pixels; ++i) {
            _coverage[i] = ((src[i / 8] >> (7 - (i % 8))) & 1) ? 255 : 0;
        }
    }

    _glyph.pixels = _coverage;
    _glyph.width = g->width;
    _glyph.height = g->height;
    _glyph.xoff = g->left;
    _glyph.yoff = -g->top;
    _glyph.advance = g->advanceX;

    return &_glyph;
}
