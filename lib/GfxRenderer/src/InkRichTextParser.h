#pragma once

#include "TextBlock.h"
#include <EpdFontFamily.h>
#include <string>
#include <vector>

class InkRichTextParser {
public:
  // Parses a string containing HTML tags (<i>, <b>) or Markdown (*, **)
  // into a vector of TextRuns, stripping the tags.
  static std::vector<TextRun> parse(const char* text);
};
