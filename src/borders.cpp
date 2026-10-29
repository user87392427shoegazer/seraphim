#include "borders.h"

static Rect box(double x, double y, double w, double h)
{
	Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}

void border_layout(const Rect *geom, unsigned inner_px, unsigned outer_px,
		BorderRects *out)
{
	double ix = inner_px, ox = outer_px;
	double x = geom->x, y = geom->y, w = geom->w, h = geom->h;

	out->inner[BORDER_TOP] = box(x - ix, y - ix, w + 2 * ix, ix);
	out->inner[BORDER_BOTTOM] = box(x - ix, y + h, w + 2 * ix, ix);
	out->inner[BORDER_LEFT] = box(x - ix, y, ix, h);
	out->inner[BORDER_RIGHT] = box(x + w, y, ix, h);

	out->outer[BORDER_TOP] = box(x - ix - ox, y - ix - ox, w + 2 * ix + 2 * ox, ox);
	out->outer[BORDER_BOTTOM] = box(x - ix - ox, y + h + ix, w + 2 * ix + 2 * ox, ox);
	out->outer[BORDER_LEFT] = box(x - ix - ox, y - ix, ox, h + 2 * ix);
	out->outer[BORDER_RIGHT] = box(x + w + ix, y - ix, ox, h + 2 * ix);
}
