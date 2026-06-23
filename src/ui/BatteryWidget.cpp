// EENK — BatteryWidget implementation
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
  // Ensure the UI font is registered in this renderer instance.
  // insertFont is idempotent if the same ID is already present; calling it
  // here keeps BatteryWidget self-contained without requiring the caller to
  // pre-register the font.
  _renderer.insertFont(kBatteryFontId, s_batFamily);
#endif

  // 'ink' = true means draw black pixels; false = white (for inverted strip).
  bool ink = !inverted;

  // ── 1. Battery body outline ───────────────────────────────────────────────
  // 24 wide × 14 tall, origin at (x, y).
  static constexpr int kBodyW = 24;
  static constexpr int kBodyH = 14;
  _renderer.drawRect(x, y, kBodyW, kBodyH, ink);

  // ── 2. Terminal nub ───────────────────────────────────────────────────────
  // 3 wide × 6 tall, centred vertically on the right edge of the body.
  static constexpr int kNubW = 3;
  static constexpr int kNubH = 6;
  static constexpr int kNubY = (kBodyH - kNubH) / 2; // = 4
  _renderer.fillRect(x + kBodyW, y + kNubY, kNubW, kNubH, ink);

  // ── 3. Charge fill ────────────────────────────────────────────────────────
  // Interior is (kBodyW - 4) px wide, (kBodyH - 4) px tall (2 px inset on
  // all sides so the outline remains visible).
  static constexpr int kCellInset = 2;
  static constexpr int kCellW = kBodyW - 2 * kCellInset; // 20
  static constexpr int kCellH = kBodyH - 2 * kCellInset; // 10

  uint16_t pct = _cachedPct;
  if (pct > 100)
    pct = 100;

  int fillW = (kCellW * pct) / 100;
  if (fillW > 0) {
    _renderer.fillRect(x + kCellInset, y + kCellInset, fillW, kCellH, ink);
  }

  // ── 4. Percentage label ───────────────────────────────────────────────────
  // Placed to the LEFT of the body so it never clips the right edge.
  // Vertically align label with body centre. The ui_10 ascender is ~10px,
  // but the font renders slightly lower, so we subtract 8px to align it.
  int labelY = y + (kBodyH - 10) / 2 - 8;

  // Build label: "~85%" (charging) or "85%" (discharging / unknown).
  char label[12];
  if (_cachedCharging) {
    snprintf(label, sizeof(label), "~%u%%", static_cast<unsigned>(pct));
  } else {
    snprintf(label, sizeof(label), "%u%%", static_cast<unsigned>(pct));
  }

#if defined(PLATFORM_ESP32) || defined(PIO_UNIT_TESTING)
  // Measure text width so we can right-justify it flush against the body.
  static constexpr int kLabelGap = 4;
  int textW = _renderer.getTextWidth(kBatteryFontId, label);
  int labelX = x - kLabelGap - textW;
  _renderer.drawText(kBatteryFontId, labelX, labelY, label, ink);
#else
  (void)labelY;
  (void)label;
#endif
}
