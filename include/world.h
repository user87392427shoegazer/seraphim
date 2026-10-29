#ifndef SERAPHIM_WORLD_H
#define SERAPHIM_WORLD_H

#include <string>
#include <vector>

#include "canvas.h"

enum WindowState {
	WSTATE_UNFOCUSED = 0,
	WSTATE_FOCUSED = 1,
	WSTATE_URGENT = 2
};

enum { WEDGE_LEFT = 1, WEDGE_RIGHT = 2, WEDGE_TOP = 4, WEDGE_BOTTOM = 8 };

struct Window {
	int id = 0;
	Rect geom = {0, 0, 640, 480};
	bool mapped = false;
	bool urgent = false;
	bool close_request = false;
	int resize_edges = 0;
	std::string title;
	std::string app_id;
	void *host = nullptr;

	WindowState state(const struct World *w) const;
};

enum { BOOKMARK_SLOTS = 10 };
struct Bookmark {
	bool set = false;
	double scale = 1.0;
	double pan_x = 0.0;
	double pan_y = 0.0;
};

struct World {
	std::vector<Window> windows;
	Camera camera;
	double view_w = 0, view_h = 0;

	int focused_id = 0;
	int next_id = 1;

	bool region_active = false;
	double rx1 = 0, ry1 = 0, rx2 = 0, ry2 = 0;

	double magnet_dist = 16;
	double align_dist = 24;
	double gap = 0;

	Rect pending_spawn = {0, 0, 0, 0};
	bool spawn_request = false;

	bool warp_request = false;
	double warp_x = 0, warp_y = 0;

	int opened = 0;
	Bookmark bookmarks[BOOKMARK_SLOTS];

	std::vector<std::string> spawn_queue;
	bool quit_request = false;
	bool reload_request = false;

	std::string ipc_out;

	World();

	int count() const;
	Window *at(int index);
	Window *find(int id);
	int index_of(int id);

	int add(const Rect &geom, void *host = nullptr);
	void remove(int id);
	void raise(int id);
	void focus(int id);
	Window *focused();

	int window_at_point(double wx, double wy);
};

void world_set_active(World *w);
World *world_get(void);

void world_spawn(World *w, const Rect &geom);

bool world_take_spawn_request(World *w);

bool world_take_pending_geometry(World *w, Rect *out);

void world_request_close(World *w, int id);

bool world_take_warp(World *w, double *x, double *y);

int world_take_close(World *w);

Rect world_default_geometry(World *w);
void world_queue_spawn(World *w, const std::string &cmd);
bool world_take_queued_spawn(World *w, std::string *cmd);
bool world_bookmark_save(World *w, int slot);
bool world_bookmark_goto(World *w, int slot);

#endif
