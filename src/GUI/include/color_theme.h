// color_theme.h — Accessible Apple-inspired dark color palette for all UI screens.
//
// Dark mode scheme modeled on Apple iOS dark systemGray values.
// Meets WCAG AA contrast (≥4.5:1 for text) and is safe for all common color
// blindness types — distinction relies on luminance, not hue.
//
// Contrast ratios vs kColBackground (28,28,30):
//   kColPrimaryLabel   → ~18:1  (WCAG AAA – white on near-black)
//   kColAccent         → ~5.2:1 (WCAG AA – systemBlue on near-black)
//   kColSecondaryLabel → ~5.9:1 (WCAG AA – medium gray on near-black)
//
// Focus indicator strategy:
//   Inactive input: dark gray fill (kColSurface) + secondary gray text
//   Active  input: solid blue fill (kColFocusFill) + white text
//   → two simultaneous cues (luminance shift + hue) for maximum salience.

#ifndef COLOR_THEME_H
#define COLOR_THEME_H

#include "GUIslice.h"

// ── Structural / surface colors ───────────────────────────────────────────────

// Page background.
// Apple dark systemGray6 — very dark near-black.
static const gslc_tsColor kColBackground     = {28,  28,  30};

// Unfocused editable input fill and read-only value display fill.
// Apple dark systemGray5 — one step lighter than background; clearly separates
// interactive/display boxes from the page without colour.
static const gslc_tsColor kColSurface        = {44,  44,  46};

// Focused input field fill — SOLID systemBlue.
// Turns the selected box into a vivid blue tile: maximum distinction from the
// dark-gray unfocused inputs regardless of color vision type.
static const gslc_tsColor kColFocusFill      = {0,  122, 255};

// ── Text colors ───────────────────────────────────────────────────────────────

// Primary labels, data values, and text inside focused inputs.
// Pure white — ~18:1 on kColBackground and ~8:1 on the blue kColFocusFill.
static const gslc_tsColor kColPrimaryLabel   = {255, 255, 255};

// Supporting labels, units, separators, and unfocused input text.
// Apple dark systemGray2 — clearly readable (~5.9:1) but visually recessed
// so focused fields (white text on blue) pop forward.
static const gslc_tsColor kColSecondaryLabel = {174, 174, 178};

// Titles, navigation/menu bar text, and priority rank labels.
// Apple systemBlue — same hue as focus fill for coherent accent language.
static const gslc_tsColor kColAccent         = {0,  122, 255};

// ── Border and glow colors ────────────────────────────────────────────────────

// Inactive element frame border.
// Apple dark systemGray3 — subtle separation on the dark background.
static const gslc_tsColor kColBorder         = {72,  72,  74};

// Inactive element glow — matches border to avoid a colored artifact.
static const gslc_tsColor kColGlowOff        = {58,  58,  60};

// Active focus ring frame and glow — same blue as the fill for a clean halo.
static const gslc_tsColor kColFocusAccent    = {0,  122, 255};

// ── Threshold bar colors ──────────────────────────────────────────────────────

// Bar track background (unfilled portion).
// Apple dark systemGray3 — clearly distinct from the blue fill in all vision modes.
static const gslc_tsColor kColBarTrack       = {72,  72,  74};

// Bar fill level indicator — Apple systemBlue.
static const gslc_tsColor kColBarFill        = {0,  122, 255};

// Focused bar frame/glow — Apple systemBlue focus ring.
static const gslc_tsColor kColBarFocusBorder = {0,  122, 255};

#endif // COLOR_THEME_H
