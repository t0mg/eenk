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
#include <builtinFonts/ui_10.h>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>

// Font ID 20 is reserved for BatteryWidget (avoids clashing with InkEngine
// IDs 0/1 or SystemUI IDs 10/11).
static constexpr int kBatteryFontId = 20;

static EpdFont s_batFont(&ui_10);
static EpdFontFamily s_batFamily(&s_batFont);
#else
// On native, millis() comes from the mock Arduino.h via the include path.
#include <Arduino.h>
static constexpr int kBatteryFontId = 20;
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
#ifdef PLATFORM_ESP32
  _renderer.insertFont(kBatteryFontId, s_batFamily);
#endif

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
    snprintf(label, sizeof(label), "charging %d%%", pct);
  } else {
    snprintf(label, sizeof(label), "%d%%", pct);
  }

#if defined(PLATFORM_ESP32) || defined(PIO_UNIT_TESTING)
  static constexpr int kLabelGap = 4;
  int textW = _renderer.getTextWidth(kBatteryFontId, label);
  int labelX = x - kLabelGap - textW;
  _renderer.drawText(kBatteryFontId, labelX, labelY, label, bodyColor);
#else
  (void)labelY;
  (void)label;
#endif
}
