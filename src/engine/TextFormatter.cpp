#include "TextFormatter.h"
#include <cstring>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────
TextFormatter::TextFormatter() : _count(0) {}

// ─────────────────────────────────────────────────────────────────────────────
void TextFormatter::clear()
{
    _count = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
void TextFormatter::pushLine(const char* line, int len)
{
    if (_count >= FMT_MAX_LINES) return; // buffer full — oldest lines lost
    if (len > FMT_MAX_COLS) len = FMT_MAX_COLS;
    FormattedLine& fl = _lines[_count++];
    if (len > 0) {
        memcpy(fl.text, line, static_cast<std::size_t>(len));
    }
    fl.text[len] = '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
void TextFormatter::append(const char* raw, int maxCols)
{
    if (!raw || maxCols <= 0) return;

    // Strip trailing newline / carriage-return
    int rawLen = static_cast<int>(strlen(raw));
    while (rawLen > 0 && (raw[rawLen - 1] == '\n' || raw[rawLen - 1] == '\r')) {
        --rawLen;
    }

    if (rawLen == 0) {
        // Blank line — push an empty row to preserve spacing
        pushLine("", 0);
        return;
    }

    // Word-wrap loop
    int pos = 0;
    while (pos < rawLen) {
        // How many characters fit on this line?
        int remaining = rawLen - pos;
        if (remaining <= maxCols) {
            // Remainder fits — push it and we're done
            pushLine(raw + pos, remaining);
            break;
        }

        // Find the last space within the column limit to break on
        int breakAt = maxCols;
        for (int i = maxCols - 1; i >= 0; --i) {
            if (isspace((unsigned char)raw[pos + i])) {
                breakAt = i;
                break;
            }
        }

        // If no space found, hard-break at maxCols (long word / URL)
        pushLine(raw + pos, breakAt);
        pos += breakAt;

        // Skip the space we broke on
        while (pos < rawLen && isspace((unsigned char)raw[pos])) ++pos;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
const char* TextFormatter::getLine(int index) const
{
    if (index < 0 || index >= _count) return nullptr;
    return _lines[index].text;
}

// ─────────────────────────────────────────────────────────────────────────────
void TextFormatter::getVisibleWindow(int visibleRows,
                                     int* outStart, int* outCount) const
{
    if (_count <= visibleRows) {
        *outStart = 0;
        *outCount = _count;
    } else {
        *outStart = _count - visibleRows;
        *outCount = visibleRows;
    }
}
