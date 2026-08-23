// eenk — NeuStyle
// Central design tokens for the neubrutalist UI language.
//
// All UI components reference these constants to ensure visual consistency.
// When translating the design to eenky (Electron/Vue), mirror these values
// in a CSS custom properties file or JS constants module.
#pragma once

namespace NeuStyle {

// ── Border & Shadow ──────────────────────────────────────────────────────────
constexpr int BORDER_W = 6;       // Thick box border (dialog, card, dividers)
constexpr int SHADOW_OFFSET = 10; // Drop shadow X and Y offset (px)

// ── Pill ─────────────────────────────────────────────────────────────────────
// Pills are fixed-height inverted rounded-rect labels used for actions.
// The semicircle caps are precomputed for PILL_RADIUS to avoid runtime trig.
constexpr int PILL_H = 32;      // Total pill height (px)
constexpr int PILL_RADIUS = 16; // Half of PILL_H — semicircle radius
constexpr int PILL_PADDING_X =
    0; // Horizontal padding inside pill (text to cap edge)

// ── Layout ───────────────────────────────────────────────────────────────────
constexpr int HEADER_H = 40;  // Top bar height (px)
constexpr int FOOTER_H = 64;  // Bottom bar height (px)
constexpr int ROW_H = 60;     // Settings/library row height (px)
constexpr int DIALOG_W = 300;      // Default dialog width (px)
constexpr int MENU_DIALOG_W = 390; // Default menu modal width (px)
constexpr int MARGIN_X = 12;       // Standard horizontal margin (px)

// ── Font IDs ─────────────────────────────────────────────────────────────────
// Heading/action font (Syne Bold 10pt, all caps)
constexpr int FONT_HEADING = 12;
// Body font (existing ui_12)
constexpr int FONT_BODY = 10;
// Body bold (existing ui_bold_12)
constexpr int FONT_BODY_BOLD = 11;
// Small font (existing ui_10)
constexpr int FONT_SMALL = 13;

// ── Bezel ────────────────────────────────────────────────────────────────────
constexpr int BEZEL_OFFSET_Y = 4; // Offset to clear top display bezel

} // namespace NeuStyle
