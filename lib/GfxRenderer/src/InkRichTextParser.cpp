#include "InkRichTextParser.h"
#include <cstring>

static bool startsWith(const char* str, const char* prefix) {
  size_t len = strlen(prefix);
  return strncmp(str, prefix, len) == 0;
}

std::vector<TextRun> InkRichTextParser::parse(const char* text) {
  std::vector<TextRun> runs;
  if (!text) return runs;

  bool isBold = false;
  bool isItalic = false;

  auto getStyle = [&]() {
    if (isBold && isItalic) return EpdFontFamily::BOLD_ITALIC;
    if (isBold) return EpdFontFamily::BOLD;
    if (isItalic) return EpdFontFamily::ITALIC;
    return EpdFontFamily::REGULAR;
  };

  std::string currentText = "";
  const char* p = text;

  auto flush = [&]() {
    if (!currentText.empty()) {
      runs.emplace_back(currentText, getStyle());
      currentText.clear();
    }
  };

  while (*p != '\0') {
    if (startsWith(p, "<br/>")) {
      currentText += '\n';
      p += 5;
    } else if (startsWith(p, "<br />")) {
      currentText += '\n';
      p += 6;
    } else if (startsWith(p, "<br>")) {
      currentText += '\n';
      p += 4;
    } else if (startsWith(p, "<b>") || startsWith(p, "<strong>")) {
      flush();
      isBold = true;
      p += (p[1] == 's') ? 8 : 3;
    } else if (startsWith(p, "</b>") || startsWith(p, "</strong>")) {
      flush();
      isBold = false;
      p += (p[2] == 's') ? 9 : 4;
    } else if (startsWith(p, "<i>") || startsWith(p, "<em>")) {
      flush();
      isItalic = true;
      p += (p[1] == 'e') ? 4 : 3;
    } else if (startsWith(p, "</i>") || startsWith(p, "</em>")) {
      flush();
      isItalic = false;
      p += (p[2] == 'e') ? 5 : 4;
    } else if (startsWith(p, "**") || startsWith(p, "__")) {
      flush();
      isBold = !isBold;
      p += 2;
    } else if (*p == '*' || *p == '_') {
      flush();
      isItalic = !isItalic;
      p += 1;
    } else {
      currentText += *p;
      p++;
    }
  }

  flush();
  return runs;
}
