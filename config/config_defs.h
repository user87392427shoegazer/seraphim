#ifndef SERAPHIM_CONFIG_DEFS_H
#define SERAPHIM_CONFIG_DEFS_H

#define COLOR(c) { \
	(float)(((c) >> 24) & 0xff) / 255.0f, \
	(float)(((c) >> 16) & 0xff) / 255.0f, \
	(float)(((c) >>  8) & 0xff) / 255.0f, \
	(float)(((c)      ) & 0xff) / 255.0f }

#if defined(__GNUC__) || defined(__clang__)
#define SERAPHIM_UNUSED __attribute__((unused))
#else
#define SERAPHIM_UNUSED
#endif

#endif
