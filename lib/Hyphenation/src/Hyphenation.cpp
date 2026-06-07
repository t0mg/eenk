#include "Hyphenation.h"

#include "Hyphenator.h"

namespace Hyphenation {

std::vector<BreakInfo> breakOffsets(const std::string& word, bool includeFallback) {
  auto internalBreaks = Hyphenator::breakOffsets(word, includeFallback);
  std::vector<BreakInfo> result;
  result.reserve(internalBreaks.size());
  for (const auto& b : internalBreaks) {
    result.push_back({b.byteOffset, b.requiresInsertedHyphen});
  }
  return result;
}

void setLanguage(const std::string& lang) { Hyphenator::setPreferredLanguage(lang); }

}  // namespace Hyphenation
