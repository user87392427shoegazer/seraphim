#include "world.h"

#include "config.h"

#include <cstdio>
#include <cstring>

static Rect rect_of(double x, double y, double w, double h)
{
	Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}

static World *g_active = nullptr;

void world_set_active(World *w)
{
	g_active = w;
}

World *world_get(void)
{
	return g_active;
}

WindowState Window::state(const World *w) const
{
	if (urgent)
		return WSTATE_URGENT;
	if (w && w->focused_id == id)
		return WSTATE_FOCUSED;
	return WSTATE_UNFOCUSED;
}

World::World()
{
	camera_init(&camera, ZOOMMIN, ZOOMMAX);
	magnet_dist = magnet_snap_dist;
	align_dist = align_snap_dist;
	gap = WINDOW_GAP;
}

int World::count() const
{
	return (int)windows.size();
}

Window *World::at(int index)
{
	if (index < 0 || index >= (int)windows.size())
		return nullptr;
	return &windows[(size_t)index];
}

Window *World::find(int id)
{
	if (id <= 0)
		return nullptr;
	for (size_t i = 0; i < windows.size(); i++) {
		if (windows[i].id == id)
			return &windows[i];
	}
	return nullptr;
}

int World::index_of(int id)
{
	for (size_t i = 0; i < windows.size(); i++) {
		if (windows[i].id == id)
			return (int)i;
	}
	return -1;
}

int World::add(const Rect &geom, void *host)
{
	Window win;
	win.id = next_id++;
	win.geom = geom;
	win.mapped = true;
	win.host = host;
	windows.push_back(win);
	opened++;
	return win.id;
}

void World::remove(int id)
{
	int i = index_of(id);
	if (i < 0)
		return;
	windows.erase(windows.begin() + i);
	if (focused_id == id) {
		focused_id = 0;
		if (!windows.empty())
			focused_id = windows.back().id;
	}
}

void World::raise(int id)
{
	int i = index_of(id);
	if (i < 0 || i == (int)windows.size() - 1)
		return;
	Window win = windows[(size_t)i];
	windows.erase(windows.begin() + i);
	windows.push_back(win);
}

void World::focus(int id)
{
	if (!find(id))
		return;
	focused_id = id;
	raise(id);
}

Window *World::focused()
{
	return find(focused_id);
}

int World::window_at_point(double wx, double wy)
{
	double pad = (double)(innerborderpx + outerborderpx);

	for (int i = (int)windows.size() - 1; i >= 0; i--) {
		Window *win = &windows[(size_t)i];
		if (!win->mapped)
			continue;
		if (wx >= win->geom.x - pad && wx <= win->geom.x + win->geom.w + pad &&
				wy >= win->geom.y - pad && wy <= win->geom.y + win->geom.h + pad)
			return win->id;
	}
	return 0;
}

Rect world_default_geometry(World *w)
{
	double ww = w->view_w * new_window_w_frac;
	double wh = w->view_h * new_window_h_frac;
	double step = new_window_cascade * (double)(w->opened % 5);
	Rect vis;
	Rect r;

	if (ww < (double)min_window_w)
		ww = (double)min_window_w;
	if (wh < (double)min_window_h)
		wh = (double)min_window_h;

	camera_visible_rect(&w->camera, w->view_w, w->view_h, &vis);

	(void)r;
	return rect_of(vis.x + (vis.w - ww) / 2.0 + step,
			vis.y + (vis.h - wh) / 2.0 + step, ww, wh);
}

void world_spawn(World *w, const Rect &geom)
{
	if (geom.w > 0.0 && geom.h > 0.0)
		w->pending_spawn = geom;
	else
		w->pending_spawn = world_default_geometry(w);
	w->spawn_request = true;
}

bool world_take_spawn_request(World *w)
{
	if (!w->spawn_request)
		return false;
	w->spawn_request = false;
	return true;
}

bool world_take_pending_geometry(World *w, Rect *out)
{
	if (w->pending_spawn.w <= 0.0 || w->pending_spawn.h <= 0.0)
		return false;
	*out = w->pending_spawn;
	w->pending_spawn = rect_of(0.0, 0.0, 0.0, 0.0);
	return true;
}

void world_request_close(World *w, int id)
{
	Window *win = w->find(id);
	if (win)
		win->close_request = true;
}

bool world_take_warp(World *w, double *x, double *y)
{
	if (!w->warp_request)
		return false;
	w->warp_request = false;
	*x = w->warp_x;
	*y = w->warp_y;
	return true;
}

int world_take_close(World *w)
{
	for (size_t i = 0; i < w->windows.size(); i++) {
		if (w->windows[i].close_request) {
			w->windows[i].close_request = false;
			return w->windows[i].id;
		}
	}
	return 0;
}

void world_queue_spawn(World *w, const std::string &cmd)
{
	if (!cmd.empty())
		w->spawn_queue.push_back(cmd);
}

bool world_take_queued_spawn(World *w, std::string *cmd)
{
	if (w->spawn_queue.empty())
		return false;
	*cmd = w->spawn_queue.front();
	w->spawn_queue.erase(w->spawn_queue.begin());
	return true;
}

bool world_bookmark_save(World *w, int slot)
{
	if (slot < 0 || slot >= BOOKMARK_SLOTS)
		return false;
	w->bookmarks[slot].set = true;
	w->bookmarks[slot].scale = w->camera.scale;
	w->bookmarks[slot].pan_x = w->camera.pan_x;
	w->bookmarks[slot].pan_y = w->camera.pan_y;
	return true;
}

bool world_bookmark_goto(World *w, int slot)
{
	if (slot < 0 || slot >= BOOKMARK_SLOTS || !w->bookmarks[slot].set)
		return false;
	w->camera.scale = w->bookmarks[slot].scale;
	w->camera.pan_x = w->bookmarks[slot].pan_x;
	w->camera.pan_y = w->bookmarks[slot].pan_y;
	return true;
}
