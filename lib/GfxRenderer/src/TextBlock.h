#pragma once
#include <EpdFontFamily.h>
#include <string>
#include <vector>

// Represents a run of text with a specific style
struct TextRun {
  std::string text;
  EpdFontFamily::Style style;
  
  TextRun(const std::string& t, EpdFontFamily::Style s = EpdFontFamily::REGULAR)
      : text(t), style(s) {}
};

// Represents a single wrapped line composed of multiple styled runs
class TextBlock {
public:
  std::vector<TextRun> runs;
  
  TextBlock() = default;
  TextBlock(const std::string& text) {
      if (!text.empty()) {
          runs.emplace_back(text, EpdFontFamily::REGULAR);
      }
  }
  ~TextBlock() = default;
  
  std::string getText() const {
      std::string result;
      for (const auto& run : runs) {
          result += run.text;
      }
      return result;
  }
  
  void addRun(const std::string& text, EpdFontFamily::Style style) {
    if (!text.empty()) {
      runs.emplace_back(text, style);
    }
  }
  
  bool isEmpty() const {
    return runs.empty();
  }
};
