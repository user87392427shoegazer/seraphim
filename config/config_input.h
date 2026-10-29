/*
 * How dragging behaves.
 */
#ifndef SERAPHIM_CONFIG_INPUT_H
#define SERAPHIM_CONFIG_INPUT_H

/* Magnet snapping: how close (in screen pixels) a dragged window has to come
 * to a screen edge or to another window's edge before it jumps onto it. */
static const float magnet_snap_dist = 16.0f;

/* Once a window has snapped onto a neighbour's edge, its other axis is pulled
 * in line with that neighbour while it stays within this distance. */
static const float align_snap_dist = 24.0f;

/* Sloppy focus: the window under the pointer gets keyboard focus without
 * having to be clicked, and is not raised. Set to false for click-to-focus. */
static const bool sloppy_focus = true;

/* Smallest window a drag is allowed to produce. */
static const unsigned int min_window_w = 80;
static const unsigned int min_window_h = 60;

/* A window opened without a spawn region gets this fraction of the visible
 * canvas, centred, and each following one is offset by the cascade step so
 * they do not land exactly on top of each other. */
static const double new_window_w_frac = 0.45;
static const double new_window_h_frac = 0.45;
static const double new_window_cascade = 32.0;

/* Key repeat: characters per second once repeating starts, and the delay in
 * milliseconds before it starts. */
static const int key_repeat_rate = 50;
static const int key_repeat_delay = 150;

#endif
