#pragma once

#include <cstddef>
#include <cstdint>

struct SerializedHyphenationPatterns {
  size_t rootOffset;
  const std::uint8_t* data;
  size_t size;
};
