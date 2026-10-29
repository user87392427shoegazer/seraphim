#include "canvas.h"

void camera_init(Camera *c, double min_scale, double max_scale)
{
	c->scale = 1.0;
	c->pan_x = 0.0;
	c->pan_y = 0.0;
	c->min_scale = min_scale;
	c->max_scale = max_scale;
}

double camera_to_screen_x(const Camera *c, double wx)
{
	return wx * c->scale + c->pan_x;
}

double camera_to_screen_y(const Camera *c, double wy)
{
	return wy * c->scale + c->pan_y;
}

double camera_to_world_x(const Camera *c, double sx)
{
	return (sx - c->pan_x) / c->scale;
}

double camera_to_world_y(const Camera *c, double sy)
{
	return (sy - c->pan_y) / c->scale;
}

void camera_pan_by(Camera *c, double dsx, double dsy)
{
	c->pan_x += dsx;
	c->pan_y += dsy;
}

void camera_set_scale_at(Camera *c, double new_scale, double ax, double ay)
{
	double wx, wy;

	if (new_scale < c->min_scale)
		new_scale = c->min_scale;
	if (new_scale > c->max_scale)
		new_scale = c->max_scale;

	wx = camera_to_world_x(c, ax);
	wy = camera_to_world_y(c, ay);
	c->scale = new_scale;
	c->pan_x = ax - wx * new_scale;
	c->pan_y = ay - wy * new_scale;
}

void camera_zoom_at(Camera *c, double delta, double ax, double ay)
{
	camera_set_scale_at(c, c->scale + delta, ax, ay);
}

void camera_visible_rect(const Camera *c, double view_w, double view_h, Rect *out)
{
	out->x = camera_to_world_x(c, 0.0);
	out->y = camera_to_world_y(c, 0.0);
	out->w = view_w / c->scale;
	out->h = view_h / c->scale;
}
