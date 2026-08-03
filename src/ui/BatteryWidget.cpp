// eenk — BatteryWidget implementation
// Renders a compact battery icon + percentage label using GfxRenderer.
//
// Icon geometry (all coords relative to the (x, y) origin passed to draw()):
//   Body:    rect  at (0, 0),            24 wide × 14 tall
//   Nub:     rect  at (24, 4),            3 wide ×  6 tall
//   Cells:   fillRect inside body at (2, 2), width proportional to pct, 10 tall
//   Label:   drawText at (30, 0)          e.g. "85%" or "~85%" when charging
//
// The BatteryMonitor is polled at most once every POLL_INTERVAL_MS to avoid
// hammering the ADC or I²C bus on every render tick.
#include "BatteryWidget.h"

#ifdef PLATFORM_ESP32
#include <BatteryMonitor.h>
#endif
#include <GfxRenderer.h>
#include "NeuStyle.h"

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#else
// On native, millis() comes from the mock Arduino.h via the include path.
#include <Arduino.h>
#endif

// ─── Construction ────────────────────────────────────────────────────────────

BatteryWidget::BatteryWidget(GfxRenderer &renderer, BatteryMonitor &battery)
    : _renderer(renderer), _battery(battery) {}

// ─── tick() ──────────────────────────────────────────────────────────────────

void BatteryWidget::tick() {
  unsigned long now = millis();
  if (_lastPollMs == 0 || (now - _lastPollMs) > POLL_INTERVAL_MS) {
    _cachedPct = _battery.readSmoothedPercentage();
    _cachedCharging = _battery.isCharging();
    _lastPollMs = now;
  }
}

// ─── draw() ──────────────────────────────────────────────────────────────────

void BatteryWidget::draw(int x, int y, bool inverted) {

  // Neubrutalist battery: white body on black header, black depletion overlay.
  // When inverted=true (default on black header), body is white, depletion is
  // black.
  bool bodyColor = !inverted; // true = white pixels on black bg
  bool deplColor = inverted;  // true = black overlay for depleted portion

  // ── 1. Battery body
  // ─────────────────────────────────────────────────────────
  _renderer.fillRect(x, y, kBodyW, kBodyH, bodyColor);
  // add stem
  _renderer.fillRect(x + kBodyW, y + 4, kStem, kBodyH - 8, bodyColor);

  // ── 2. Depletion overlay
  // ──────────────────────────────────────────────────── Black rect from right
  // edge inward, proportional to (100 - pct).
#ifdef PIO_UNIT_TESTING
  uint32_t pct = 66;
#else
  uint32_t pct = _cachedPct;
#endif
  if (pct > 100)
    pct = 100;

  int depletedW = (kBodyW - 2 * kPadding) * (100 - pct) / 100;
  if (depletedW > 0) {
    _renderer.fillRect(x + kBodyW - kPadding - depletedW, y + kPadding,
                       depletedW, kBodyH - 2 * kPadding, deplColor);
  }

  // ── 3. Percentage label
  // ─────────────────────────────────────────────────────
  int labelY = y + (kBodyH - 10) / 2 - 8;
  char label[32];

#ifdef PIO_UNIT_TESTING
  bool isCharging = true;
#else
  bool isCharging = _battery.isCharging();
#endif

  if (isCharging) {
    snprintf(label, sizeof(label), "CHRG %d%%", pct);
  } else {
    snprintf(label, sizeof(label), "%d%%", pct);
  }

#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE) || defined(PIO_UNIT_TESTING)
  static constexpr int kLabelGap = 4;
  int textW = _renderer.getTextWidth(NeuStyle::FONT_HEADING, label);
  int labelX = x - kLabelGap - textW;
  _renderer.drawText(NeuStyle::FONT_HEADING, labelX, labelY, label, bodyColor);
#else
  (void)labelY;
  (void)label;
#endif
}
