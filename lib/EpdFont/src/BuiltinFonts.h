// eenk — BuiltinFonts
// Canonical table of built-in typefaces.
//
// Each entry maps a token name (used in @font: hints and settings) to
// four EpdFontData pointers (regular / bold / italic / boldItalic).
// nullptr variants fall back to the nearest available style at render time.
//
// Token naming convention:
//   <family>-<size>   e.g. "sans-xsmall", "sans-medium", "serif-medium"
//   Short aliases are also registered for convenience: "sans", "serif".
//
// SD card fonts are NOT in this table — they are resolved dynamically.
#pragma once
#include <EpdFontData.h>
#include <cstddef>
#include <cstring>

// ── Table entry ──────────────────────────────────────────────────────────────

struct BuiltinFontEntry {
    const char*        token;       // @font: token (e.g. "sans-medium")
    const char*        displayName; // shown in SettingsView
    const EpdFontData* regular;     // always non-null for shipped entries
    const EpdFontData* bold;        // nullptr → falls back to regular at render
    const EpdFontData* italic;      // nullptr → falls back to regular at render
    const EpdFontData* boldItalic;  // nullptr → falls back to bold or italic
};

// ── Table ────────────────────────────────────────────────────────────────────

extern const BuiltinFontEntry kBuiltinFonts[];
extern const size_t kBuiltinFontCount;

// Index of the default sans-medium entry.
static constexpr size_t kDefaultBuiltinFontIndex = 0;

// ── Lookup helpers ───────────────────────────────────────────────────────────

/// Find a builtin entry by token string. Returns nullptr if not found.
inline const BuiltinFontEntry* findBuiltinByToken(const char* token) {
    if (!token || token[0] == '\0') return nullptr;
    for (size_t i = 0; i < kBuiltinFontCount; ++i) {
#ifdef PLATFORM_NATIVE
        if (_stricmp(kBuiltinFonts[i].token, token) == 0)
#else
        if (strcasecmp(kBuiltinFonts[i].token, token) == 0)
#endif
            return &kBuiltinFonts[i];
    }
    return nullptr;
}

/// Get a builtin entry by index (clamped to valid range).
inline const BuiltinFontEntry* getBuiltinByIndex(size_t index) {
    if (index >= kBuiltinFontCount) index = kDefaultBuiltinFontIndex;
    return &kBuiltinFonts[index];
}
