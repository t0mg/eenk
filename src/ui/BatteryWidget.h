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
  struct Status {
    bool supported = true;
    bool percentageKnown = true;
    bool millivoltsKnown = true;
    bool chargingKnown = true;
    uint16_t percentage = 100;
    uint16_t millivolts = 4200;
    bool charging = false;
  };

  uint16_t readPercentage() const { return 100; }
  bool isCharging() const { return false; }
  uint16_t readMillivolts() const { return 4200; }
  Status readStatus() const { return {}; }
};
#endif

class BatteryWidget {
public:
  BatteryWidget(GfxRenderer &renderer, BatteryMonitor &battery);

  // Draw the battery icon + % at position (x, y).
  // Width is ~40px, height is ~16px.
  // Black on white, or white on black if inverted=true.
  void draw(int x, int y, bool inverted = false);

  // Throttled poll — reads the battery at most every POLL_INTERVAL_MS (60 s).
  // Returns true if percentage or charging state changed.
  bool tick(); // call from main loop

  // Force an immediate re-read, bypassing the throttle (e.g. the Settings
  // Battery row). Updates cached percentage/charging/millivolts.
  void refreshNow();

  static constexpr int getWidth() { return kBodyW + kStem; }
  static constexpr int getHeight() { return kBodyH; }

  uint16_t getPercentage() const { return _cachedPct; }
  uint16_t getMillivolts() const { return _cachedMv; }
  bool isCharging() const { return _cachedCharging; }
  bool hasStateChanged() const { return _stateChanged; }

  // Piecewise-linear 1% Li-ion percentage curve for ADC-based boards (e.g. X4),
  // smoothing out the 10% notch jumps from BatteryMonitor::percentageFromMillivolts.
  static uint16_t percentageFromMillivolts(uint16_t millivolts);

private:
  // Single poll of percentage + charging + millivolts; updates the cache and
  // sets _stateChanged when percentage or charging changed (millivolt drift
  // alone must not trigger redraws).
  void pollOnce();

  static constexpr int kBodyW = 32;
  static constexpr int kBodyH = 16;
  static constexpr int kPadding = 2;
  static constexpr int kStem = 4;
  GfxRenderer &_renderer;
  BatteryMonitor &_battery;
  uint16_t _cachedPct = 100;
  uint16_t _cachedMv = 4200;
  bool _cachedCharging = false;
  bool _stateChanged = false;
  unsigned long _lastPollMs = 0;
  unsigned long _lastChargePollMs = 0;
  static constexpr unsigned long POLL_INTERVAL_MS = 60000;
  static constexpr unsigned long CHARGE_POLL_INTERVAL_MS = 500;
};
