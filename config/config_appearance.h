/*
 * How seraphim looks.
 *
 * Every window carries two independent rings drawn around its surface: an
 * inner border hugging the content and an outer border wrapped around that.
 * Both have their own width and their own colour per state, so you can stack
 * a loud outer ring around a quiet inner one, or switch a ring off by setting
 * its width to 0.
 */
#ifndef SERAPHIM_CONFIG_APPEARANCE_H
#define SERAPHIM_CONFIG_APPEARANCE_H

#include "config_defs.h"

/* Background of the empty canvas (0xRRGGBBAA). */
static const float rootcolor[] = COLOR(0x1b1b1bff);

/* --- Window borders -------------------------------------------------- */
static const unsigned int innerborderpx = 2;
static const unsigned int outerborderpx = 2;

/* Focused window: the one the keyboard is talking to. */
static const float focused_inner_border_color[]   = COLOR(0x8899aaff);
static const float focused_outer_border_color[]   = COLOR(0x4b5a6aff);

/* Urgent window: it wants attention (an X11 client raised its hand, or a
 * script rule called window_set_urgent). */
static const float urgent_inner_border_color[]    = COLOR(0xff5555ff);
static const float urgent_outer_border_color[]    = COLOR(0x8b1a1aff);

/* Everything else. */
static const float unfocused_inner_border_color[] = COLOR(0x2e2e2eff);
static const float unfocused_outer_border_color[] = COLOR(0x1a1a1aff);

/* Gap left between windows that snap next to each other, in canvas pixels.
 * Negative values make neighbouring borders overlap. */
#define WINDOW_GAP 0

/* --- Spawn region -----------------------------------------------------
 * The rectangle you drag out on the empty canvas to choose where the next
 * window opens. */
#define SPAWNREGION_STYLE 1  /* 0 = invisible, 1 = filled, 2 = outlined */

/* Style 1: fill colour of the rectangle. */
static const float spawnregion_color[] = COLOR(0x8899aa66);

/* Style 2: the same inner/outer ring scheme as a window, drawn inside the
 * region so a dragged-out area looks like the window it will become. */
static const unsigned int spawnregion_innerborderpx = 2;
static const unsigned int spawnregion_outerborderpx = 2;
static const float spawnregion_inner_border_color[] = COLOR(0x8899aaff);
static const float spawnregion_outer_border_color[] = COLOR(0x4b5a6aff);

/* A region this small (canvas pixels) counts as a click, not a drag. */
static const int spawnregion_min_size = 10;

#endif
