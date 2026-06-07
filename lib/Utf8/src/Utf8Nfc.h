#pragma once

#include <cstddef>

/**
 * Normalize a UTF-8 string to NFC (Canonical Composition).
 * Composes NFD-decomposed sequences (e.g., Vietnamese A+circumflex+acute → Ấ).
 * ASCII-only strings pass through with no allocation.
 *
 * Operates in-place on a char buffer. The result is always <= the input length.
 * Returns the new length (excluding null terminator). The buffer is null-terminated.
 */
size_t utf8NormalizeNfc(char* buf, size_t len);
