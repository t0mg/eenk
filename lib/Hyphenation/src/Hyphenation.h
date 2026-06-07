#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Hyphenation {

struct BreakInfo {
  size_t byteOffset;
  bool requiresInsertedHyphen;
};

std::vector<BreakInfo> breakOffsets(const std::string& word, bool includeFallback);
void setLanguage(const std::string& lang);

}  // namespace Hyphenation
