#ifndef SERAPHIM_CANVAS_H
#define SERAPHIM_CANVAS_H

struct Rect {
	double x, y, w, h;
};

struct Camera {
	double scale;
	double pan_x, pan_y;
	double min_scale, max_scale;
};

void camera_init(Camera *c, double min_scale, double max_scale);

double camera_to_screen_x(const Camera *c, double wx);
double camera_to_screen_y(const Camera *c, double wy);

double camera_to_world_x(const Camera *c, double sx);
double camera_to_world_y(const Camera *c, double sy);

void camera_pan_by(Camera *c, double dsx, double dsy);

void camera_set_scale_at(Camera *c, double new_scale, double ax, double ay);
void camera_zoom_at(Camera *c, double delta, double ax, double ay);

void camera_visible_rect(const Camera *c, double view_w, double view_h, Rect *out);

#endif
