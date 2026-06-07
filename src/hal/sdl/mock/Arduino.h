#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>

// Mock for Arduino PROGMEM
#define PROGMEM
#define IRAM_ATTR

inline uint8_t pgm_read_byte(const void* addr) {
    return *reinterpret_cast<const uint8_t*>(addr);
}

inline uint16_t pgm_read_word(const void* addr) {
    return *reinterpret_cast<const uint16_t*>(addr);
}

inline uint32_t pgm_read_dword(const void* addr) {
    return *reinterpret_cast<const uint32_t*>(addr);
}

inline const void* pgm_read_ptr(const void* addr) {
    return *reinterpret_cast<const void* const*>(addr);
}

// Map Arduino types
using String = std::string;
inline void delay(uint32_t ms) {}

inline uint32_t millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

#define MALLOC_CAP_8BIT 0
inline size_t heap_caps_get_largest_free_block(uint32_t caps) { return 1024 * 1024 * 100; }
