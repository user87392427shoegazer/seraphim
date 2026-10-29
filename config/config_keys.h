/*
 * Keys and buttons.
 *
 * seraphim's own modifier bits are mapped from the keyboard state in
 * src/main.cpp; keysyms are the XKB_KEY_* values from keysymdef.h. All of it
 * is registered into AngelScript by src/script.cpp, so scripts/seraphim.as
 * reads these names directly instead of repeating the numbers.
 */
#ifndef SERAPHIM_CONFIG_KEYS_H
#define SERAPHIM_CONFIG_KEYS_H

#include <linux/input-event-codes.h>

#define SERAPHIM_MOD_SHIFT 1
#define SERAPHIM_MOD_CTRL  4
#define SERAPHIM_MOD_ALT   8
#define SERAPHIM_MOD_LOGO  64

/* The mouse button that spawns windows on the empty canvas: click for a
 * window of the default size, drag to draw the region it opens in.
 *
 *   BTN_RIGHT   (default)
 *   BTN_MIDDLE  for people who would rather keep the right button free
 *
 * Left drag on empty canvas always pans, so do not set this to BTN_LEFT. */
#define SPAWN_BUTTON BTN_RIGHT

/* The modifier most bindings hang off: Super+left drag moves a window,
 * Super+scroll zooms, Super+arrows pan the canvas.
 *
 * SERAPHIM_MOD_LOGO is the Super/Windows/Meta key. Swap it for
 * SERAPHIM_MOD_ALT if you would rather use Alt; nothing else has to change,
 * because every binding compares against MODKEY rather than a fixed bit. */
#define MODKEY SERAPHIM_MOD_LOGO

/* The keysym values from keysymdef.h. They carry a prefix here because
 * <linux/input-event-codes.h> already uses the bare KEY_* names for scancodes,
 * which are not the same thing. src/script.cpp publishes these to the policy
 * script under their short names -- KEY_LEFT, KEY_ZOOM_IN, ... -- so
 * scripts/seraphim.as still reads well. */
#define SERAPHIM_KEY_LEFT    0xff51  /* XKB_KEY_Left */
#define SERAPHIM_KEY_UP      0xff52  /* XKB_KEY_Up */
#define SERAPHIM_KEY_RIGHT   0xff53  /* XKB_KEY_Right */
#define SERAPHIM_KEY_DOWN    0xff54  /* XKB_KEY_Down */
#define SERAPHIM_KEY_HOME    0xff50  /* XKB_KEY_Home */
#define SERAPHIM_KEY_MINUS   0x002d  /* XKB_KEY_minus */
#define SERAPHIM_KEY_EQUAL   0x003d  /* XKB_KEY_equal */
#define SERAPHIM_KEY_0       0x0030  /* XKB_KEY_0 */
#define SERAPHIM_KEY_Q       0x0071  /* XKB_KEY_q */
#define SERAPHIM_KEY_T       0x0074  /* XKB_KEY_t */
#define SERAPHIM_KEY_W       0x0077  /* XKB_KEY_w */
#define SERAPHIM_KEY_A       0x0061  /* XKB_KEY_a */
#define SERAPHIM_KEY_S       0x0073  /* XKB_KEY_s */
#define SERAPHIM_KEY_D       0x0064  /* XKB_KEY_d */
#define SERAPHIM_KEY_Z       0x007a  /* XKB_KEY_z */
#define SERAPHIM_KEY_TAB     0xff09  /* XKB_KEY_Tab */
#define SERAPHIM_KEY_L       0x006c  /* XKB_KEY_l */
#define SERAPHIM_KEY_R       0x0072  /* XKB_KEY_r */

/* Host-level bindings. The script never sees these: Super+Escape and
 * Super+Shift+q quit, Ctrl+Alt+F1..F12 ask the session for another VT. */
#define SERAPHIM_KEY_Q_UPPER 0x0051  /* XKB_KEY_Q, what Shift+left shifts q to */
#define SERAPHIM_KEY_ESCAPE  0xff1b  /* XKB_KEY_Escape */
#define SERAPHIM_KEY_F1      0xffbe  /* XKB_KEY_F1; F2..F12 follow in order */

#endif
