#pragma once
#include <cstddef>

/**
 * EENK — TextFormatter
 *
 * Word-wraps raw ink narrative text to fit the display's character grid.
 * Outputs a simple fixed-size line buffer (no STL, no heap allocation per
 * line) suitable for both native and ESP32 targets.
 *
 * Constraints:
 *   MAX_COLS  — maximum characters per line (set by display columns)
 *   MAX_LINES — maximum lines in the scrollback buffer
 *   MAX_LINE_LEN — max chars stored per line (same as MAX_COLS + nul)
 */

static constexpr int FMT_MAX_COLS     = 48;  // matches SDLDisplay::COLS
static constexpr int FMT_MAX_LINES    = 512; // scrollback buffer depth
static constexpr int FMT_MAX_LINE_LEN = FMT_MAX_COLS + 1;

struct FormattedLine
{
    char text[FMT_MAX_LINE_LEN];
};

class TextFormatter
{
public:
    TextFormatter();

    /**
     * Append a raw ink output line to the scrollback buffer, word-wrapping
     * as needed to fit within maxCols columns.
     * @param raw     Null-terminated string from InkCPP getline_alloc()
     * @param maxCols Override column width (defaults to FMT_MAX_COLS)
     */
    void append(const char* raw, int maxCols = FMT_MAX_COLS);

    /** Discard all lines in the buffer. */
    void clear();

    /** Total number of formatted lines currently in the buffer. */
    int lineCount() const { return _count; }

    /**
     * Access a formatted line by absolute index.
     * Returns nullptr if index is out of range.
     */
    const char* getLine(int index) const;

    /**
     * Return a view of the last `visibleRows` lines — what the display
     * should currently show as the narrative text area.
     * @param visibleRows   How many rows are available for narrative text
     * @param outStart      Set to the first line index of the visible window
     * @param outCount      Set to how many lines are in the visible window
     */
    void getVisibleWindow(int visibleRows, int* outStart, int* outCount) const;

private:
    FormattedLine _lines[FMT_MAX_LINES];
    int           _count = 0;

    /** Append a single already-wrapped line to the buffer. */
    void pushLine(const char* line, int len);
};
