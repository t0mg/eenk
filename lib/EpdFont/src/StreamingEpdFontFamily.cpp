#include "StreamingEpdFontFamily.h"

#include <cstring>
#include <cstdio>

// ── Construction / destruction ─────────────────────────────────────────────

StreamingEpdFontFamily::StreamingEpdFontFamily() {
  for (int i = 0; i < 4; ++i) _slots[i] = nullptr;
}

StreamingEpdFontFamily::~StreamingEpdFontFamily() { unload(); }

void StreamingEpdFontFamily::unload() {
  for (int i = 0; i < 4; ++i) {
    delete _slots[i];
    _slots[i] = nullptr;
  }
}

// ── Internal helpers ───────────────────────────────────────────────────────

/*static*/
StreamingEpdFont* StreamingEpdFontFamily::tryLoadFromDirs(const char* const* dirs,
                                                          const char* const* candidateNames) {
  char path[MAX_PATH];
  for (const char* const* d = dirs; *d != nullptr; ++d) {
    for (const char* const* n = candidateNames; *n != nullptr; ++n) {
      int written = snprintf(path, sizeof(path), "%s/%s", *d, *n);
      if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) continue;
      auto* sf = new StreamingEpdFont();
      if (sf->load(path)) {
        printf("[StreamingEpdFontFamily] Loaded: %s\n", path);
        return sf;
      }
      delete sf;
    }
  }
  return nullptr;
}

// ── load() ─────────────────────────────────────────────────────────────────

// Helper to strip style suffixes from font stem (case-insensitive)
static void extractBaseStem(const char* stem, char* out, size_t outSize) {
  strncpy(out, stem, outSize - 1);
  out[outSize - 1] = '\0';
  size_t len = strlen(out);

  const char* suffixes[] = {
    "-bolditalic", "-bold-italic", "-bold_italic",
    "-regular", "-regula", "-bold", "-italic", "-medium",
    nullptr
  };

  for (const char** s = suffixes; *s; ++s) {
    size_t slen = strlen(*s);
    if (len > slen) {
      bool match = true;
      for (size_t i = 0; i < slen; ++i) {
        if (tolower(static_cast<unsigned char>(out[len - slen + i])) !=
            tolower(static_cast<unsigned char>((*s)[i]))) {
          match = false;
          break;
        }
      }
      if (match) {
        out[len - slen] = '\0';
        break;
      }
    }
  }
}

bool StreamingEpdFontFamily::load(const char* stem, const char* const* dirs) {
  unload();

  if (!stem || !stem[0] || !dirs || !dirs[0]) return false;

  char baseStem[64];
  extractBaseStem(stem, baseStem, sizeof(baseStem));

  // Build per-variant candidate filenames with case variations and stem aliases.
  char fnReg1[64], fnReg2[64], fnReg3[64], fnReg4[64], fnReg5[64];
  snprintf(fnReg1, sizeof(fnReg1), "%s-regular.epdfont", baseStem);
  snprintf(fnReg2, sizeof(fnReg2), "%s-Regular.epdfont", baseStem);
  snprintf(fnReg3, sizeof(fnReg3), "%s.epdfont",         baseStem);
  snprintf(fnReg4, sizeof(fnReg4), "%s.epdfont",         stem);
  snprintf(fnReg5, sizeof(fnReg5), "%s-regular.epdfont", stem);

  char fnBold1[64], fnBold2[64], fnBold3[64];
  snprintf(fnBold1, sizeof(fnBold1), "%s-bold.epdfont", baseStem);
  snprintf(fnBold2, sizeof(fnBold2), "%s-Bold.epdfont", baseStem);
  snprintf(fnBold3, sizeof(fnBold3), "%s-bold.epdfont", stem);

  char fnItalic1[64], fnItalic2[64], fnItalic3[64];
  snprintf(fnItalic1, sizeof(fnItalic1), "%s-italic.epdfont", baseStem);
  snprintf(fnItalic2, sizeof(fnItalic2), "%s-Italic.epdfont", baseStem);
  snprintf(fnItalic3, sizeof(fnItalic3), "%s-italic.epdfont", stem);

  char fnBI1[64], fnBI2[64], fnBI3[64];
  snprintf(fnBI1, sizeof(fnBI1), "%s-bolditalic.epdfont", baseStem);
  snprintf(fnBI2, sizeof(fnBI2), "%s-BoldItalic.epdfont", baseStem);
  snprintf(fnBI3, sizeof(fnBI3), "%s-bold-italic.epdfont", baseStem);

  const char* regularCandidates[] = { fnReg1, fnReg2, fnReg3, fnReg4, fnReg5, nullptr };
  _slots[EpdFontFamily::REGULAR] = tryLoadFromDirs(dirs, regularCandidates);
  if (!_slots[EpdFontFamily::REGULAR]) {
    printf("[StreamingEpdFontFamily] Regular not found for stem '%s' (base '%s')\n", stem, baseStem);
    return false;
  }

  const char* boldCandidates[]       = { fnBold1, fnBold2, fnBold3, nullptr };
  const char* italicCandidates[]     = { fnItalic1, fnItalic2, fnItalic3, nullptr };
  const char* boldItalicCandidates[] = { fnBI1, fnBI2, fnBI3, nullptr };

  _slots[EpdFontFamily::BOLD]        = tryLoadFromDirs(dirs, boldCandidates);
  _slots[EpdFontFamily::ITALIC]      = tryLoadFromDirs(dirs, italicCandidates);
  _slots[EpdFontFamily::BOLD_ITALIC] = tryLoadFromDirs(dirs, boldItalicCandidates);

  printf("[StreamingEpdFontFamily] Loaded family '%s' (base '%s'): B=%d I=%d BI=%d\n",
         stem, baseStem,
         _slots[EpdFontFamily::BOLD]        != nullptr ? 1 : 0,
         _slots[EpdFontFamily::ITALIC]      != nullptr ? 1 : 0,
         _slots[EpdFontFamily::BOLD_ITALIC] != nullptr ? 1 : 0);

  return true;
}

// ── resolveSlot() ──────────────────────────────────────────────────────────

StreamingEpdFont* StreamingEpdFontFamily::resolveSlot(EpdFontFamily::Style style) const {
  StreamingEpdFont* sf = _slots[static_cast<int>(style)];
  if (sf) return sf;
  // Fallback chain
  if (style == EpdFontFamily::BOLD_ITALIC) {
    sf = _slots[EpdFontFamily::BOLD];
    if (sf) return sf;
  }
  if (style == EpdFontFamily::ITALIC || style == EpdFontFamily::BOLD_ITALIC) {
    sf = _slots[EpdFontFamily::ITALIC];
    if (sf) return sf;
  }
  return _slots[EpdFontFamily::REGULAR];  // always non-null when isLoaded()
}

// ── Delegate accessors ─────────────────────────────────────────────────────

const EpdFontData* StreamingEpdFontFamily::getData(EpdFontFamily::Style style) const {
  StreamingEpdFont* sf = resolveSlot(style);
  return sf ? sf->getData() : nullptr;
}

const EpdGlyph* StreamingEpdFontFamily::getGlyph(uint32_t cp, EpdFontFamily::Style style) const {
  StreamingEpdFont* sf = resolveSlot(style);
  return sf ? sf->getGlyph(cp) : nullptr;
}

const uint8_t* StreamingEpdFontFamily::getGlyphBitmap(const EpdGlyph* glyph,
                                                       EpdFontFamily::Style style) const {
  StreamingEpdFont* sf = resolveSlot(style);
  return sf ? sf->getGlyphBitmap(glyph) : nullptr;
}
