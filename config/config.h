/*
 * seraphim's master config.
 *
 * Only src/main.cpp needs this file: it is the one that knows how to launch
 * programs. The camera, drag and colour knobs live in the other config
 * headers and are pushed into the AngelScript policy at startup.
 */
#ifndef SERAPHIM_CONFIG_H
#define SERAPHIM_CONFIG_H

#include "config_appearance.h"
#include "config_animation.h"
#include "config_input.h"
#include "config_keys.h"

/* What gets launched when you drag a spawn region out on the empty canvas, or
 * right-click it. NULL-terminated argv. */
static const char *spawncmd[] SERAPHIM_UNUSED = { "foot", NULL };

#endif
