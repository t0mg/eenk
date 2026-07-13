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

bool StreamingEpdFontFamily::load(const char* stem, const char* const* dirs) {
  unload();

  if (!stem || !stem[0] || !dirs || !dirs[0]) return false;

  // Build per-variant candidate filenames.
  // Each style tries the suffixed name; REGULAR also tries the plain name.
  char fnRegularPlain[64];
  char fnRegular[64];
  char fnBold[64];
  char fnItalic[64];
  char fnBoldItalic[64];

  snprintf(fnRegularPlain, sizeof(fnRegularPlain), "%s.epdfont",           stem);
  snprintf(fnRegular,      sizeof(fnRegular),      "%s-regular.epdfont",   stem);
  snprintf(fnBold,         sizeof(fnBold),          "%s-bold.epdfont",      stem);
  snprintf(fnItalic,       sizeof(fnItalic),        "%s-italic.epdfont",    stem);
  snprintf(fnBoldItalic,   sizeof(fnBoldItalic),    "%s-bolditalic.epdfont", stem);

  // REGULAR: try "<stem>-regular.epdfont" then "<stem>.epdfont"
  const char* regularCandidates[] = { fnRegular, fnRegularPlain, nullptr };
  _slots[EpdFontFamily::REGULAR] = tryLoadFromDirs(dirs, regularCandidates);
  if (!_slots[EpdFontFamily::REGULAR]) {
    printf("[StreamingEpdFontFamily] Regular not found for stem '%s'\n", stem);
    return false;
  }

  // BOLD, ITALIC, BOLD_ITALIC: optional — missing means fallback at render time
  const char* boldCandidates[]       = { fnBold,       nullptr };
  const char* italicCandidates[]     = { fnItalic,     nullptr };
  const char* boldItalicCandidates[] = { fnBoldItalic, nullptr };

  _slots[EpdFontFamily::BOLD]       = tryLoadFromDirs(dirs, boldCandidates);
  _slots[EpdFontFamily::ITALIC]     = tryLoadFromDirs(dirs, italicCandidates);
  _slots[EpdFontFamily::BOLD_ITALIC] = tryLoadFromDirs(dirs, boldItalicCandidates);

  printf("[StreamingEpdFontFamily] Loaded family '%s': B=%d I=%d BI=%d\n",
         stem,
         _slots[EpdFontFamily::BOLD]       != nullptr ? 1 : 0,
         _slots[EpdFontFamily::ITALIC]     != nullptr ? 1 : 0,
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
