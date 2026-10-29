const int STATE_UNFOCUSED = 0;
const int STATE_FOCUSED = 1;
const int STATE_URGENT = 2;

const int DRAG_NONE = 0;
const int DRAG_MOVE = 1;
const int DRAG_RESIZE = 2;
const int DRAG_PAN = 3;
const int DRAG_REGION = 4;

const int EDGE_LEFT = 1;
const int EDGE_RIGHT = 2;
const int EDGE_TOP = 4;
const int EDGE_BOTTOM = 8;

int drag_mode = DRAG_NONE;
int drag_id = 0;
int resize_edges = 0;

float press_sx = 0.0f;
float press_sy = 0.0f;
float grab_x = 0.0f;
float grab_y = 0.0f;
float grab_w = 0.0f;
float grab_h = 0.0f;

float pan_x0 = 0.0f;
float pan_y0 = 0.0f;
float pan_sx0 = 0.0f;
float pan_sy0 = 0.0f;

bool drag_moved = false;

float absf(float v) { return v < 0.0f ? -v : v; }

bool overlap(float a1, float a2, float b1, float b2) { return a1 < b2 && b1 < a2; }

float screen_to_canvas(float px)
{
	float s = canvas_scale();
	if (s < 0.0001f) s = 1.0f;
	return px / s;
}

void camera_home()
{
	canvas_set_scale(1.0f);
	canvas_set_pan(0.0f, 0.0f);
}

int snap_ref_x = 0;
int snap_ref_y = 0;

float snap_axis_x(float nx, float nw, float ny, float nh, int self, float thresh)
{
	float vx = view_x();
	float vw = view_w();
	float best = 0.0f;
	bool have = false;
	int ref = 0;
	int i;
	float d;

	snap_ref_x = 0;

	d = nx - vx;
	if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = vx - nx; have = true; ref = -1; }
	d = (nx + nw) - (vx + vw);
	if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (vx + vw) - (nx + nw); have = true; ref = -1; }

	for (i = 0; i < window_count(); i++) {
		int o = window_id_at(i);
		float ox, oy, ow, oh;
		if (o == self) continue;
		ox = window_x(o); oy = window_y(o); ow = window_w(o); oh = window_h(o);
		if (!overlap(ny, ny + nh, oy, oy + oh)) continue;

		d = nx - ox;
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = -d; have = true; ref = o; }
		d = (nx + nw + window_gap) - ox;
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = ox - window_gap - (nx + nw); have = true; ref = o; }
		d = (nx + nw) - (ox + ow);
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (ox + ow) - (nx + nw); have = true; ref = o; }
		d = nx - (ox + ow + window_gap);
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (ox + ow + window_gap) - nx; have = true; ref = o; }
	}

	snap_ref_x = ref;
	return nx + best;
}

float snap_axis_y(float ny, float nh, float nx, float nw, int self, float thresh)
{
	float vy = view_y();
	float vh = view_h();
	float best = 0.0f;
	bool have = false;
	int ref = 0;
	int i;
	float d;

	snap_ref_y = 0;

	d = ny - vy;
	if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = vy - ny; have = true; ref = -1; }
	d = (ny + nh) - (vy + vh);
	if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (vy + vh) - (ny + nh); have = true; ref = -1; }

	for (i = 0; i < window_count(); i++) {
		int o = window_id_at(i);
		float ox, oy, ow, oh;
		if (o == self) continue;
		ox = window_x(o); oy = window_y(o); ow = window_w(o); oh = window_h(o);
		if (!overlap(nx, nx + nw, ox, ox + ow)) continue;

		d = ny - oy;
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = -d; have = true; ref = o; }
		d = (ny + nh + window_gap) - oy;
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = oy - window_gap - (ny + nh); have = true; ref = o; }
		d = (ny + nh) - (oy + oh);
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (oy + oh) - (ny + nh); have = true; ref = o; }
		d = ny - (oy + oh + window_gap);
		if (absf(d) <= thresh && (!have || absf(d) < absf(best))) { best = (oy + oh + window_gap) - ny; have = true; ref = o; }
	}

	snap_ref_y = ref;
	return ny + best;
}

float align_y_to(int o, float ny, float nh, float tol)
{
	float oy, oh, best, bd, d;
	if (o <= 0) return ny;
	oy = window_y(o); oh = window_h(o);
	best = ny; bd = tol;

	d = absf(ny - oy);
	if (d <= bd) { bd = d; best = oy; }
	d = absf((ny + nh) - (oy + oh));
	if (d <= bd) { bd = d; best = oy + oh - nh; }
	d = absf((ny + nh / 2.0f) - (oy + oh / 2.0f));
	if (d <= bd) { bd = d; best = oy + (oh - nh) / 2.0f; }
	return best;
}

float align_x_to(int o, float nx, float nw, float tol)
{
	float ox, ow, best, bd, d;
	if (o <= 0) return nx;
	ox = window_x(o); ow = window_w(o);
	best = nx; bd = tol;

	d = absf(nx - ox);
	if (d <= bd) { bd = d; best = ox; }
	d = absf((nx + nw) - (ox + ow));
	if (d <= bd) { bd = d; best = ox + ow - nw; }
	d = absf((nx + nw / 2.0f) - (ox + ow / 2.0f));
	if (d <= bd) { bd = d; best = ox + (ow - nw) / 2.0f; }
	return best;
}

void magnet_move(int id, float nx, float ny)
{
	float nw = window_w(id);
	float nh = window_h(id);
	float thresh = screen_to_canvas(magnet_snap_dist);
	float tol = screen_to_canvas(align_snap_dist);
	float x, y;
	int refx, refy;

	x = snap_axis_x(nx, nw, ny, nh, id, thresh);
	refx = snap_ref_x;
	y = snap_axis_y(ny, nh, x, nw, id, thresh);
	refy = snap_ref_y;

	if (refy == 0 && refx > 0)
		y = align_y_to(refx, y, nh, tol);
	if (refx == 0 && refy > 0)
		x = align_x_to(refy, x, nw, tol);

	window_move(id, x, y);
}

bool point_in_content(int id, float wx, float wy)
{
	float x = window_x(id), y = window_y(id);
	float w = window_w(id), h = window_h(id);
	return wx >= x && wx <= x + w && wy >= y && wy <= y + h;
}

int resize_edges_for(int id, float sx, float sy)
{
	float x0 = canvas_to_screen_x(window_x(id));
	float y0 = canvas_to_screen_y(window_y(id));
	float x1 = canvas_to_screen_x(window_x(id) + window_w(id));
	float y1 = canvas_to_screen_y(window_y(id) + window_h(id));
	int edges = 0;

	edges |= (sx < (x0 + x1) * 0.5f) ? EDGE_LEFT : EDGE_RIGHT;
	edges |= (sy < (y0 + y1) * 0.5f) ? EDGE_TOP : EDGE_BOTTOM;
	return edges;
}

void resize_grab_point(int id, int edges, float sx, float sy, float &out tx, float &out ty)
{
	float x0 = canvas_to_screen_x(window_x(id));
	float y0 = canvas_to_screen_y(window_y(id));
	float x1 = canvas_to_screen_x(window_x(id) + window_w(id));
	float y1 = canvas_to_screen_y(window_y(id) + window_h(id));

	tx = sx;
	ty = sy;
	if ((edges & EDGE_LEFT) != 0) tx = x0;
	else if ((edges & EDGE_RIGHT) != 0) tx = x1;
	if ((edges & EDGE_TOP) != 0) ty = y0;
	else if ((edges & EDGE_BOTTOM) != 0) ty = y1;
}

void center_on(int id)
{
	float scale = canvas_scale();
	float sw = view_w() * scale;
	float sh = view_h() * scale;
	float cx = window_x(id) + window_w(id) * 0.5f;
	float cy = window_y(id) + window_h(id) * 0.5f;

	canvas_set_pan(sw * 0.5f - cx * scale, sh * 0.5f - cy * scale);
}

bool center_visible(int id)
{
	float cx = window_x(id) + window_w(id) * 0.5f;
	float cy = window_y(id) + window_h(id) * 0.5f;

	return cx >= view_x() && cx <= view_x() + view_w() &&
	       cy >= view_y() && cy <= view_y() + view_h();
}

void focus_direction(int dx, int dy)
{
	int from = window_focused();
	int best = 0;
	float best_score = 0.0f;
	float fx, fy;
	int i;

	if (from != 0) {
		fx = window_x(from) + window_w(from) * 0.5f;
		fy = window_y(from) + window_h(from) * 0.5f;
	} else {
		fx = view_center_x();
		fy = view_center_y();
	}

	for (i = 0; i < window_count(); i++) {
		int o = window_id_at(i);
		float cx, cy, along, across, score;

		if (o == from) continue;
		cx = window_x(o) + window_w(o) * 0.5f;
		cy = window_y(o) + window_h(o) * 0.5f;
		along = (cx - fx) * float(dx) + (cy - fy) * float(dy);
		if (along <= 1.0f) continue;
		across = (dx != 0) ? absf(cy - fy) : absf(cx - fx);
		score = along + 2.0f * across;
		if (best == 0 || score < best_score) { best = o; best_score = score; }
	}

	if (best == 0) return;
	window_focus(best);
	if (!center_visible(best))
		center_on(best);
}

void zoom_fit()
{
	float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
	bool have = false;
	float sw = view_w() * canvas_scale();
	float sh = view_h() * canvas_scale();
	float margin = 48.0f;
	float bw, bh, s, s2, real;
	int i;

	for (i = 0; i < window_count(); i++) {
		int o = window_id_at(i);
		float ox = window_x(o), oy = window_y(o);
		float ex = ox + window_w(o), ey = oy + window_h(o);

		if (!have) { x0 = ox; y0 = oy; x1 = ex; y1 = ey; have = true; continue; }
		if (ox < x0) x0 = ox;
		if (oy < y0) y0 = oy;
		if (ex > x1) x1 = ex;
		if (ey > y1) y1 = ey;
	}
	if (!have) return;

	bw = x1 - x0; if (bw < 1.0f) bw = 1.0f;
	bh = y1 - y0; if (bh < 1.0f) bh = 1.0f;
	s = (sw - 2.0f * margin) / bw;
	s2 = (sh - 2.0f * margin) / bh;
	if (s2 < s) s = s2;
	if (s > 1.0f) s = 1.0f;

	canvas_set_scale(s);
	real = canvas_scale();
	canvas_set_pan(sw * 0.5f - (x0 + bw * 0.5f) * real, sh * 0.5f - (y0 + bh * 0.5f) * real);
}

funcdef void LayoutFn(const array<int> &in ids, float x, float y, float w, float h);

array<string> layout_names;
array<LayoutFn@> layout_fns;
int layout_current = 0;
float layout_margin = 24.0f;

void register_layout(const string &in name, LayoutFn@ fn)
{
	for (uint i = 0; i < layout_names.length(); i++) {
		if (layout_names[i] == name) {
			@layout_fns[i] = fn;
			return;
		}
	}
	layout_names.insertLast(name);
	layout_fns.insertLast(fn);
}

int layout_index(const string &in name)
{
	for (uint i = 0; i < layout_names.length(); i++)
		if (layout_names[i] == name)
			return int(i);
	return -1;
}

void visible_windows(array<int> &result)
{
	for (int i = 0; i < window_count(); i++) {
		int id = window_id_at(i);
		if (window_in_view(id))
			result.insertLast(id);
	}
	result.sortAsc();
}

bool apply_layout_index(int index)
{
	array<int> ids;
	LayoutFn@ fn;
	float m = layout_margin;

	if (index < 0 || index >= int(layout_fns.length()))
		return false;
	@fn = layout_fns[index];
	if (fn is null)
		return false;

	visible_windows(ids);
	if (ids.length() == 0)
		return true;
	fn(ids, view_x() + m, view_y() + m, view_w() - 2.0f * m, view_h() - 2.0f * m);
	return true;
}

void cycle_layout()
{
	if (layout_names.length() == 0)
		return;
	layout_current = (layout_current + 1) % int(layout_names.length());
	log("layout: " + layout_names[layout_current]);
	apply_layout_index(layout_current);
}

void layout_grid(const array<int> &in ids, float x, float y, float w, float h)
{
	int n = int(ids.length());
	int cols = 1;
	int rows;
	float cw, ch;

	while (cols * cols < n)
		cols++;
	rows = (n + cols - 1) / cols;
	cw = (w - window_gap * float(cols - 1)) / float(cols);
	ch = (h - window_gap * float(rows - 1)) / float(rows);

	for (int i = 0; i < n; i++) {
		int c = i % cols;
		int r = i / cols;
		window_set_geom(ids[i], x + float(c) * (cw + window_gap),
				y + float(r) * (ch + window_gap), cw, ch);
	}
}

void layout_columns(const array<int> &in ids, float x, float y, float w, float h)
{
	int n = int(ids.length());
	float cw = (w - window_gap * float(n - 1)) / float(n);

	for (int i = 0; i < n; i++)
		window_set_geom(ids[i], x + float(i) * (cw + window_gap), y, cw, h);
}

void layout_rows(const array<int> &in ids, float x, float y, float w, float h)
{
	int n = int(ids.length());
	float rh = (h - window_gap * float(n - 1)) / float(n);

	for (int i = 0; i < n; i++)
		window_set_geom(ids[i], x, y + float(i) * (rh + window_gap), w, rh);
}

void layout_master(const array<int> &in ids, float x, float y, float w, float h)
{
	int n = int(ids.length());
	float mw, sw, sh;

	if (n == 1) {
		window_set_geom(ids[0], x, y, w, h);
		return;
	}
	mw = (w - window_gap) * 0.55f;
	sw = w - window_gap - mw;
	sh = (h - window_gap * float(n - 2)) / float(n - 1);

	window_set_geom(ids[0], x, y, mw, h);
	for (int i = 1; i < n; i++)
		window_set_geom(ids[i], x + mw + window_gap,
				y + float(i - 1) * (sh + window_gap), sw, sh);
}

void layout_cascade(const array<int> &in ids, float x, float y, float w, float h)
{
	int n = int(ids.length());
	float step = 40.0f;
	float cw = w * 0.6f;
	float ch = h * 0.6f;

	for (int i = 0; i < n; i++)
		window_set_geom(ids[i], x + step * float(i), y + step * float(i), cw, ch);
}

class Rule {
	string app;
	float w;
	float h;
}

array<Rule> rules;

void rule_size(const string &in app, float w, float h)
{
	Rule r;
	r.app = app;
	r.w = w;
	r.h = h;
	rules.insertLast(r);
}

void apply_rules(int id)
{
	string app = window_app_id(id);

	if (app.length() == 0)
		return;
	for (uint i = 0; i < rules.length(); i++) {
		if (rules[i].app == app) {
			window_set_geom(id, window_x(id), window_y(id), rules[i].w, rules[i].h);
			return;
		}
	}
}

void setup()
{
	register_layout("grid", layout_grid);
	register_layout("columns", layout_columns);
	register_layout("rows", layout_rows);
	register_layout("master", layout_master);
	register_layout("cascade", layout_cascade);
}

void ipc_layout(const string &in name)
{
	int index = layout_index(name);

	if (index < 0) {
		ipc_reply("error: no layout called " + name);
		return;
	}
	layout_current = index;
	apply_layout_index(index);
}

void ipc_layouts()
{
	for (uint i = 0; i < layout_names.length(); i++) {
		if (int(i) == layout_current)
			ipc_reply(layout_names[i] + " *");
		else
			ipc_reply(layout_names[i]);
	}
}

void ipc_home()
{
	camera_home();
}

void ipc_fit()
{
	zoom_fit();
}

void seraphim_init()
{
	camera_home();
	setup();
	log("desk up: Super+t opens a terminal, Super+left drags a window, Super+l tidies the windows on screen");
}

void seraphim_reload()
{
	setup();
	log("script reloaded");
}

void seraphim_window_changed(int id)
{
}

void seraphim_window_open(int id)
{
	apply_rules(id);

	float x = window_x(id), y = window_y(id);
	float w = window_w(id), h = window_h(id);
	float vw = view_w(), vh = view_h();

	if (w > vw) w = vw;
	if (h > vh) h = vh;
	if (w != window_w(id) || h != window_h(id))
		window_set_geom(id, x, y, w, h);

	window_focus(id);
}

void seraphim_window_close(int id)
{
}

bool seraphim_key(uint keysym, uint mods, bool pressed)
{
	if (!pressed)
		return false;
	if ((mods & MODKEY) == 0)
		return false;

	bool ctrl = (mods & MOD_CTRL) != 0;
	int digit = key_digit(keysym);

	if (digit > 0) {
		if (ctrl)
			bookmark_save(digit);
		else
			bookmark_goto(digit);
		return true;
	}
	if (keysym == KEY_R && ctrl) { reload_script(); return true; }
	if (keysym == KEY_L) {
		if (ctrl)
			cycle_layout();
		else
			apply_layout_index(layout_current);
		return true;
	}

	if (keysym == KEY_LEFT) { canvas_pan_by(canvas_pan_step, 0.0f); return true; }
	if (keysym == KEY_RIGHT) { canvas_pan_by(-canvas_pan_step, 0.0f); return true; }
	if (keysym == KEY_UP) { canvas_pan_by(0.0f, canvas_pan_step); return true; }
	if (keysym == KEY_DOWN) { canvas_pan_by(0.0f, -canvas_pan_step); return true; }
	if (keysym == KEY_EQUAL) { canvas_zoom(zoom_step); return true; }
	if (keysym == KEY_MINUS) { canvas_zoom(-zoom_step); return true; }
	if (keysym == KEY_0 || keysym == KEY_HOME) { camera_home(); return true; }
	if (keysym == KEY_T) { window_spawn(0.0f, 0.0f, 0.0f, 0.0f); return true; }
	if (keysym == KEY_W) { focus_direction(0, -1); return true; }
	if (keysym == KEY_S) { focus_direction(0, 1); return true; }
	if (keysym == KEY_A) { focus_direction(-1, 0); return true; }
	if (keysym == KEY_D) { focus_direction(1, 0); return true; }
	if (keysym == KEY_Z) { zoom_fit(); return true; }
	if (keysym == KEY_TAB) {
		int focused = window_focused();
		if (focused != 0)
			center_on(focused);
		return true;
	}
	if (keysym == KEY_Q) {
		int focused = window_focused();
		if (focused != 0)
			window_close(focused);
		return true;
	}
	return false;
}

void seraphim_pointer_axis(float delta, float sx, float sy, uint mods)
{
	if (delta == 0.0f)
		return;
	float dir = delta > 0.0f ? 1.0f : -1.0f;
	canvas_zoom_at(dir * zoom_step, sx, sy);
}

void seraphim_pointer_motion(float sx, float sy, int hit, uint mods)
{
	float scale = canvas_scale();
	if (scale < 0.0001f) scale = 1.0f;

	if (drag_mode == DRAG_NONE)
		return;

	if (drag_mode == DRAG_PAN) {
		canvas_set_pan(pan_x0 + (sx - pan_sx0), pan_y0 + (sy - pan_sy0));
		drag_moved = true;
		return;
	}

	if (drag_mode == DRAG_REGION) {
		region_update(sx, sy);
		drag_moved = true;
		return;
	}

	if (drag_mode == DRAG_MOVE) {
		float nx = grab_x + (sx - press_sx) / scale;
		float ny = grab_y + (sy - press_sy) / scale;
		magnet_move(drag_id, nx, ny);
		drag_moved = true;
		return;
	}

	if (drag_mode == DRAG_RESIZE) {
		float nx = grab_x, ny = grab_y, nw = grab_w, nh = grab_h;

		if ((resize_edges & EDGE_LEFT) != 0) {
			float d = (sx - press_sx) / scale;
			nx = grab_x + d;
			nw = grab_w - d;
		} else if ((resize_edges & EDGE_RIGHT) != 0) {
			nw = grab_w + (sx - press_sx) / scale;
		}
		if ((resize_edges & EDGE_TOP) != 0) {
			float d = (sy - press_sy) / scale;
			ny = grab_y + d;
			nh = grab_h - d;
		} else if ((resize_edges & EDGE_BOTTOM) != 0) {
			nh = grab_h + (sy - press_sy) / scale;
		}

		if (nw < float(min_window_w)) {
			if ((resize_edges & EDGE_LEFT) != 0)
				nx = grab_x + grab_w - float(min_window_w);
			nw = float(min_window_w);
		}
		if (nh < float(min_window_h)) {
			if ((resize_edges & EDGE_TOP) != 0)
				ny = grab_y + grab_h - float(min_window_h);
			nh = float(min_window_h);
		}

		window_set_geom(drag_id, nx, ny, nw, nh);
		drag_moved = true;
		return;
	}
}

bool seraphim_pointer_button(int button, bool pressed, float sx, float sy, int hit, uint mods)
{
	if (pressed) {
		press_sx = sx;
		press_sy = sy;
		drag_moved = false;
		drag_mode = DRAG_NONE;
		drag_id = 0;
		resize_edges = 0;

		if ((button == BTN_MIDDLE && (mods & MODKEY) != 0) ||
				(button == BTN_LEFT && (mods & MODKEY) != 0 && (mods & MOD_ALT) != 0)) {
			drag_mode = DRAG_PAN;
			pan_sx0 = sx;
			pan_sy0 = sy;
			pan_x0 = canvas_pan_x();
			pan_y0 = canvas_pan_y();
			return true;
		}

		if (hit != 0) {
			bool on_border = !point_in_content(hit, canvas_to_world_x(sx), canvas_to_world_y(sy));

			if (button == BTN_RIGHT && (mods & MODKEY) != 0) {
				float tx, ty;
				drag_mode = DRAG_RESIZE;
				resize_edges = resize_edges_for(hit, sx, sy);
				resize_grab_point(hit, resize_edges, sx, sy, tx, ty);
				cursor_warp(tx, ty);
				press_sx = tx;
				press_sy = ty;
				window_set_resize_edges(hit, resize_edges);
			} else if (button == BTN_LEFT && ((mods & MODKEY) != 0 || on_border)) {
				drag_mode = DRAG_MOVE;
			} else {
				window_raise(hit);
				window_focus(hit);
				return false;
			}

			drag_id = hit;
			window_raise(hit);
			window_focus(hit);
			grab_x = window_x(hit);
			grab_y = window_y(hit);
			grab_w = window_w(hit);
			grab_h = window_h(hit);
			return true;
		}

		if (button == spawn_button) {
			drag_mode = DRAG_REGION;
			region_begin(sx, sy);
			return true;
		}
		if (button == BTN_LEFT) {
			drag_mode = DRAG_PAN;
			pan_sx0 = sx;
			pan_sy0 = sy;
			pan_x0 = canvas_pan_x();
			pan_y0 = canvas_pan_y();
			return true;
		}
		return false;
	}

	int was = drag_mode;
	if (was == DRAG_RESIZE && drag_id != 0)
		window_set_resize_edges(drag_id, 0);
	drag_mode = DRAG_NONE;
	drag_id = 0;
	resize_edges = 0;

	if (was == DRAG_REGION) {
		float x, y, w, h;
		if (region_finish(x, y, w, h)) {
			return true;
		}
		region_cancel();

		if (button == spawn_button && !drag_moved)
			window_spawn(0.0f, 0.0f, 0.0f, 0.0f);
		return true;
	}

	return was != DRAG_NONE;
}
