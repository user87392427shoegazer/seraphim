#ifndef SERAPHIM_BORDERS_H
#define SERAPHIM_BORDERS_H

#include "canvas.h"

#define BORDER_TOP    0
#define BORDER_BOTTOM 1
#define BORDER_LEFT   2
#define BORDER_RIGHT  3

struct BorderRects {
	Rect inner[4];
	Rect outer[4];
};

void border_layout(const Rect *geom, unsigned inner_px, unsigned outer_px,
		BorderRects *out);

#endif
