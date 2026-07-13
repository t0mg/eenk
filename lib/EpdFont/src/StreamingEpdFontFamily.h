// EENK — StreamingEpdFontFamily
// Owns up to 4 StreamingEpdFont instances for a single font family on SD card.
//
// File naming convention (fontconvert.py compatible):
//   <stem>.epdfont            ← alias for <stem>-regular.epdfont (plain fallback)
//   <stem>-regular.epdfont   ← required
//   <stem>-bold.epdfont      ← optional
//   <stem>-italic.epdfont    ← optional
//   <stem>-bolditalic.epdfont ← optional
//
// Fallback chain (mirrors builtin EpdFontFamily):
//   BOLD_ITALIC → BOLD → REGULAR
//   ITALIC      → REGULAR
//   BOLD        → REGULAR
//
// Search paths tried in order (first match wins):
//   sidecar: /eenk/<storyBase>/<stem>[suffix].epdfont
//   shared:  /fonts/<stem>[suffix].epdfont
#pragma once

#include "EpdFontFamily.h"
#include "StreamingEpdFont.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

class StreamingEpdFontFamily {
 public:
  StreamingEpdFontFamily();
  ~StreamingEpdFontFamily();

  StreamingEpdFontFamily(const StreamingEpdFontFamily&) = delete;
  StreamingEpdFontFamily& operator=(const StreamingEpdFontFamily&) = delete;

  /// Load a font family by stem name, searching the given null-terminated
  /// list of directory paths in order (first match for each variant wins).
  /// At least the regular variant must be found for load() to succeed.
  ///
  /// @param stem  Bare name, e.g. "palatino" or "reader-small"
  /// @param dirs  Null-terminated array of dir paths, e.g.
  ///              { "/eenk/mystory", "/fonts", nullptr }
  /// @return true if the regular variant was loaded
  bool load(const char* stem, const char* const* dirs);

  void unload();

  bool isLoaded() const { return _slots[EpdFontFamily::REGULAR] != nullptr; }

  bool hasStyle(EpdFontFamily::Style style) const {
    return _slots[static_cast<int>(style)] != nullptr;
  }

  // ── Delegate accessors ────────────────────────────────────────────────────

  const EpdFontData* getData(EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  const EpdGlyph*    getGlyph(uint32_t cp,
                               EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  const uint8_t*     getGlyphBitmap(const EpdGlyph* glyph,
                                    EpdFontFamily::Style style) const;

  /// Return the StreamingEpdFont* to use for a given style (applying the
  /// fallback chain). Returns nullptr only when not loaded at all.
  StreamingEpdFont* resolveSlot(EpdFontFamily::Style style) const;

  /// Direct slot access (may be nullptr if that variant wasn't found).
  StreamingEpdFont* slot(EpdFontFamily::Style style) const {
    return _slots[static_cast<int>(style)];
  }

 private:
  StreamingEpdFont* _slots[4];  // indexed by EpdFontFamily::Style

  static constexpr size_t MAX_PATH = 256;

  /// Try each dir in turn to find any of candidateNames[].
  /// Returns a loaded StreamingEpdFont on success, nullptr otherwise.
  static StreamingEpdFont* tryLoadFromDirs(const char* const* dirs,
                                           const char* const* candidateNames);
};
