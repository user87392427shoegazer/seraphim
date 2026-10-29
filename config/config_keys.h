#ifndef SERAPHIM_CONFIG_KEYS_H
#define SERAPHIM_CONFIG_KEYS_H

#include <linux/input-event-codes.h>

#define SERAPHIM_MOD_SHIFT 1
#define SERAPHIM_MOD_CTRL  4
#define SERAPHIM_MOD_ALT   8
#define SERAPHIM_MOD_LOGO  64

#define SPAWN_BUTTON BTN_RIGHT

#define MODKEY SERAPHIM_MOD_LOGO

#define SERAPHIM_KEY_LEFT    0xff51
#define SERAPHIM_KEY_UP      0xff52
#define SERAPHIM_KEY_RIGHT   0xff53
#define SERAPHIM_KEY_DOWN    0xff54
#define SERAPHIM_KEY_HOME    0xff50
#define SERAPHIM_KEY_MINUS   0x002d
#define SERAPHIM_KEY_EQUAL   0x003d
#define SERAPHIM_KEY_0       0x0030
#define SERAPHIM_KEY_Q       0x0071
#define SERAPHIM_KEY_T       0x0074
#define SERAPHIM_KEY_W       0x0077
#define SERAPHIM_KEY_A       0x0061
#define SERAPHIM_KEY_S       0x0073
#define SERAPHIM_KEY_D       0x0064
#define SERAPHIM_KEY_Z       0x007a
#define SERAPHIM_KEY_TAB     0xff09
#define SERAPHIM_KEY_L       0x006c
#define SERAPHIM_KEY_R       0x0072

#define SERAPHIM_KEY_Q_UPPER 0x0051
#define SERAPHIM_KEY_ESCAPE  0xff1b
#define SERAPHIM_KEY_F1      0xffbe

#endif
