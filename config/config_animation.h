/*
 * Camera settings.
 *
 * seraphim deliberately has no inertia and no tag animations (Super+z fits
 * everything on screen, in the script):
 * the canvas zooms with the scroll wheel and everything is applied at once.
 * ZOOMMIN/ZOOMMAX are the hard limits the wheel and the keyboard run into.
 */
#ifndef SERAPHIM_CONFIG_ANIMATION_H
#define SERAPHIM_CONFIG_ANIMATION_H

#define ZOOMMIN 0.30  /* most zoomed out the canvas can get */
#define ZOOMMAX 1.00  /* 1.0 = 100%; raise it to zoom past native size */

/* How much one wheel notch changes the zoom. */
#define ZOOMSTEP 0.10

/* How far one Super+arrow key press pans the canvas, in screen pixels. */
#define CANVAS_PAN_STEP 120.0

#endif
