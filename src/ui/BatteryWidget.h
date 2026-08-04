// eenk — BatteryWidget
// Renders a compact battery icon and percentage label in a status-bar strip.
// Uses GfxRenderer primitives (drawRect / fillRect / drawText).
//
// Layout (at origin x, y):
//   [  body 24×14  ][nub 3×6]  "85%" or "⚡85%"
//   Total footprint: ~40px wide, ~14px tall
#pragma once
#include <cstdint>

class GfxRenderer;
#ifdef PLATFORM_ESP32
class BatteryMonitor;
#else
class BatteryMonitor {
public:
  uint16_t readPercentage() const { return 100; }
  bool isCharging() const { return false; }
};
#endif

class BatteryWidget {
public:
  BatteryWidget(GfxRenderer &renderer, BatteryMonitor &battery);

  // Draw the battery icon + % at position (x, y).
  // Width is ~40px, height is ~16px.
  // Black on white, or white on black if inverted=true.
  void draw(int x, int y, bool inverted = false);

  // Quick poll — only reads battery every 30 s to avoid ADC overhead.
  void tick(); // call from main loop

  static constexpr int getWidth() { return kBodyW + kStem; }
  static constexpr int getHeight() { return kBodyH; }

  uint16_t getPercentage() const { return _cachedPct; }
  bool isCharging() const { return _cachedCharging; }

private:
  static constexpr int kBodyW = 32;
  static constexpr int kBodyH = 16;
  static constexpr int kPadding = 2;
  static constexpr int kStem = 4;
  GfxRenderer &_renderer;
  BatteryMonitor &_battery;
  uint16_t _cachedPct = 100;
  bool _cachedCharging = false;
  unsigned long _lastPollMs = 0;
  static constexpr unsigned long POLL_INTERVAL_MS = 30000;
};
