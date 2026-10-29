#ifndef SERAPHIM_CONFIG_APPEARANCE_H
#define SERAPHIM_CONFIG_APPEARANCE_H

#include "config_defs.h"

static const float rootcolor[] = COLOR(0x1b1b1bff);

static const unsigned int innerborderpx = 2;
static const unsigned int outerborderpx = 2;

static const float focused_inner_border_color[]   = COLOR(0x8899aaff);
static const float focused_outer_border_color[]   = COLOR(0x4b5a6aff);

static const float urgent_inner_border_color[]    = COLOR(0xff5555ff);
static const float urgent_outer_border_color[]    = COLOR(0x8b1a1aff);

static const float unfocused_inner_border_color[] = COLOR(0x2e2e2eff);
static const float unfocused_outer_border_color[] = COLOR(0x1a1a1aff);

#define WINDOW_GAP 0

#define SPAWNREGION_STYLE 1

static const float spawnregion_color[] = COLOR(0x8899aa66);

static const unsigned int spawnregion_innerborderpx = 2;
static const unsigned int spawnregion_outerborderpx = 2;
static const float spawnregion_inner_border_color[] = COLOR(0x8899aaff);
static const float spawnregion_outer_border_color[] = COLOR(0x4b5a6aff);

static const int spawnregion_min_size = 10;

#endif
