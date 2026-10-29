/*
 * Helpers shared by every seraphim config header.
 *
 * These headers must stay free of wlroots includes: src/script.cpp and the
 * test in tests/ include them too, and neither links wlroots.
 */
#ifndef SERAPHIM_CONFIG_DEFS_H
#define SERAPHIM_CONFIG_DEFS_H

/* 0xRRGGBBAA -> a brace-enclosed float array initialiser. Valid in both C and
 * C++, so the same config compiles everywhere it is used:
 *     static const float rootcolor[] = COLOR(0x1b1b1bff); */
#define COLOR(c) { \
	(float)(((c) >> 24) & 0xff) / 255.0f, \
	(float)(((c) >> 16) & 0xff) / 255.0f, \
	(float)(((c) >>  8) & 0xff) / 255.0f, \
	(float)(((c)      ) & 0xff) / 255.0f }

/* For config values that a translation unit pulls in but never reads: the desk
 * model includes config.h as well and never launches anything, which would
 * otherwise warn about an unused spawncmd. */
#if defined(__GNUC__) || defined(__clang__)
#define SERAPHIM_UNUSED __attribute__((unused))
#else
#define SERAPHIM_UNUSED
#endif

#endif
