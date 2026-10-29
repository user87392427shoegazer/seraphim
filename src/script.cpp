#include "script.h"

#include "borders.h"
#include "canvas.h"
#include "world.h"

#include "config_animation.h"
#include "config_appearance.h"
#include "config_input.h"
#include "config_keys.h"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scriptstdstring/scriptstdstring.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static unsigned cfg_modkey = MODKEY;
static unsigned cfg_mod_alt = SERAPHIM_MOD_ALT;
static int cfg_btn_left = BTN_LEFT;
static int cfg_btn_middle = BTN_MIDDLE;
static int cfg_btn_right = BTN_RIGHT;

static unsigned cfg_key_left = SERAPHIM_KEY_LEFT;
static unsigned cfg_key_up = SERAPHIM_KEY_UP;
static unsigned cfg_key_right = SERAPHIM_KEY_RIGHT;
static unsigned cfg_key_down = SERAPHIM_KEY_DOWN;
static unsigned cfg_key_home = SERAPHIM_KEY_HOME;
static unsigned cfg_key_minus = SERAPHIM_KEY_MINUS;
static unsigned cfg_key_equal = SERAPHIM_KEY_EQUAL;
static unsigned cfg_key_0 = SERAPHIM_KEY_0;
static unsigned cfg_key_q = SERAPHIM_KEY_Q;
static unsigned cfg_key_t = SERAPHIM_KEY_T;
static unsigned cfg_key_w = SERAPHIM_KEY_W;
static unsigned cfg_key_a = SERAPHIM_KEY_A;
static unsigned cfg_key_s = SERAPHIM_KEY_S;
static unsigned cfg_key_d = SERAPHIM_KEY_D;
static unsigned cfg_key_z = SERAPHIM_KEY_Z;
static unsigned cfg_key_tab = SERAPHIM_KEY_TAB;
static unsigned cfg_key_l = SERAPHIM_KEY_L;
static unsigned cfg_key_r = SERAPHIM_KEY_R;
static unsigned cfg_mod_ctrl = SERAPHIM_MOD_CTRL;
static unsigned cfg_mod_shift = SERAPHIM_MOD_SHIFT;
static int cfg_spawn_button = SPAWN_BUTTON;

static float cfg_magnet_dist = magnet_snap_dist;
static float cfg_align_dist = align_snap_dist;
static float cfg_window_gap = (float)WINDOW_GAP;
static float cfg_zoom_min = (float)ZOOMMIN;
static float cfg_zoom_max = (float)ZOOMMAX;
static float cfg_zoom_step = (float)ZOOMSTEP;
static float cfg_pan_step = (float)CANVAS_PAN_STEP;
static int cfg_min_window_w = (int)min_window_w;
static int cfg_min_window_h = (int)min_window_h;

static World *world()
{
	World *w = world_get();
	return w;
}

static Rect rect_of(double x, double y, double w, double h)
{
	Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}

static float canvas_scale(void)
{
	World *w = world();
	return w ? (float)w->camera.scale : 1.0f;
}

static void canvas_set_scale(float s)
{
	World *w = world();
	if (!w)
		return;
	camera_set_scale_at(&w->camera, s, w->view_w / 2.0, w->view_h / 2.0);
}

static void canvas_set_scale_at(float s, float ax, float ay)
{
	World *w = world();
	if (!w)
		return;
	camera_set_scale_at(&w->camera, s, ax, ay);
}

static void canvas_zoom(float delta)
{
	World *w = world();
	if (!w)
		return;
	camera_zoom_at(&w->camera, delta, w->view_w / 2.0, w->view_h / 2.0);
}

static void canvas_zoom_at(float delta, float ax, float ay)
{
	World *w = world();
	if (!w)
		return;
	camera_zoom_at(&w->camera, delta, ax, ay);
}

static float canvas_pan_x(void)
{
	World *w = world();
	return w ? (float)w->camera.pan_x : 0.0f;
}

static float canvas_pan_y(void)
{
	World *w = world();
	return w ? (float)w->camera.pan_y : 0.0f;
}

static void canvas_set_pan(float px, float py)
{
	World *w = world();
	if (!w)
		return;
	w->camera.pan_x = px;
	w->camera.pan_y = py;
}

static void canvas_pan_by(float dx, float dy)
{
	World *w = world();
	if (!w)
		return;
	camera_pan_by(&w->camera, dx, dy);
}

static float canvas_to_world_x(float sx)
{
	World *w = world();
	return w ? (float)camera_to_world_x(&w->camera, sx) : sx;
}

static float canvas_to_world_y(float sy)
{
	World *w = world();
	return w ? (float)camera_to_world_y(&w->camera, sy) : sy;
}

static float canvas_to_screen_x(float wx)
{
	World *w = world();
	return w ? (float)camera_to_screen_x(&w->camera, wx) : wx;
}

static float canvas_to_screen_y(float wy)
{
	World *w = world();
	return w ? (float)camera_to_screen_y(&w->camera, wy) : wy;
}

static void visible_rect(Rect *out)
{
	World *w = world();
	if (!w) {
		*out = rect_of(0, 0, 0, 0);
		return;
	}
	camera_visible_rect(&w->camera, w->view_w, w->view_h, out);
}

static float view_x(void)
{
	Rect r;
	visible_rect(&r);
	return (float)r.x;
}

static float view_y(void)
{
	Rect r;
	visible_rect(&r);
	return (float)r.y;
}

static float view_w(void)
{
	Rect r;
	visible_rect(&r);
	return (float)r.w;
}

static float view_h(void)
{
	Rect r;
	visible_rect(&r);
	return (float)r.h;
}

static float view_center_x(void)
{
	Rect r;
	visible_rect(&r);
	return (float)(r.x + r.w / 2.0);
}

static float view_center_y(void)
{
	Rect r;
	visible_rect(&r);
	return (float)(r.y + r.h / 2.0);
}

static int window_count(void)
{
	World *w = world();
	return w ? w->count() : 0;
}

static int window_id_at(int index)
{
	World *w = world();
	Window *win = w ? w->at(index) : nullptr;
	return win ? win->id : 0;
}

static Window *win_or_null(int id)
{
	World *w = world();
	return w ? w->find(id) : nullptr;
}

static float window_x(int id)
{
	Window *win = win_or_null(id);
	return win ? (float)win->geom.x : 0.0f;
}

static float window_y(int id)
{
	Window *win = win_or_null(id);
	return win ? (float)win->geom.y : 0.0f;
}

static float window_w(int id)
{
	Window *win = win_or_null(id);
	return win ? (float)win->geom.w : 0.0f;
}

static float window_h(int id)
{
	Window *win = win_or_null(id);
	return win ? (float)win->geom.h : 0.0f;
}

static void window_set_geom(int id, float x, float y, float w, float h)
{
	Window *win = win_or_null(id);
	if (!win)
		return;
	if (w < (float)min_window_w)
		w = (float)min_window_w;
	if (h < (float)min_window_h)
		h = (float)min_window_h;
	win->geom = rect_of(x, y, w, h);
}

static void window_move(int id, float x, float y)
{
	Window *win = win_or_null(id);
	if (!win)
		return;
	win->geom.x = x;
	win->geom.y = y;
}

static void window_focus(int id)
{
	World *w = world();
	if (w)
		w->focus(id);
}

static int window_focused(void)
{
	World *w = world();
	return w ? w->focused_id : 0;
}

static bool window_is_focused(int id)
{
	World *w = world();
	return w && w->focused_id == id;
}

static void window_raise(int id)
{
	World *w = world();
	if (w)
		w->raise(id);
}

static void window_set_urgent(int id, bool urgent)
{
	Window *win = win_or_null(id);
	if (win)
		win->urgent = urgent;
}

static bool window_urgent(int id)
{
	Window *win = win_or_null(id);
	return win ? win->urgent : false;
}

static void window_set_resize_edges(int id, int edges)
{
	Window *win = win_or_null(id);

	if (win)
		win->resize_edges = edges;
}

static void cursor_warp(float sx, float sy)
{
	World *w = world();

	if (!w)
		return;
	w->warp_request = true;
	w->warp_x = sx;
	w->warp_y = sy;
}

static void window_close(int id)
{
	World *w = world();
	if (w)
		world_request_close(w, id);
}

static int window_at_point(float wx, float wy)
{
	World *w = world();
	return w ? w->window_at_point(wx, wy) : 0;
}

static int window_at_screen_point(float sx, float sy)
{
	World *w = world();
	if (!w)
		return 0;
	return w->window_at_point(camera_to_world_x(&w->camera, sx),
			camera_to_world_y(&w->camera, sy));
}

static void window_spawn(float x, float y, float w, float h)
{
	World *wr = world();
	if (wr)
		world_spawn(wr, rect_of(x, y, w, h));
}

static void region_begin(float sx, float sy)
{
	World *w = world();
	if (!w)
		return;
	w->region_active = true;
	w->rx1 = sx;
	w->ry1 = sy;
	w->rx2 = sx;
	w->ry2 = sy;
}

static void region_update(float sx, float sy)
{
	World *w = world();
	if (!w || !w->region_active)
		return;
	w->rx2 = sx;
	w->ry2 = sy;
}

static void region_cancel(void)
{
	World *w = world();
	if (!w)
		return;
	w->region_active = false;
}

static bool region_active(void)
{
	World *w = world();
	return w ? w->region_active : false;
}

static bool region_finish(float &ox, float &oy, float &ow, float &oh)
{
	World *w = world();
	double w1, w2, h1, h2;

	if (!w || !w->region_active)
		return false;
	w->region_active = false;

	if (w->camera.scale <= 0.0)
		return false;

	w1 = camera_to_world_x(&w->camera, w->rx1 < w->rx2 ? w->rx1 : w->rx2);
	w2 = camera_to_world_x(&w->camera, w->rx1 < w->rx2 ? w->rx2 : w->rx1);
	h1 = camera_to_world_y(&w->camera, w->ry1 < w->ry2 ? w->ry1 : w->ry2);
	h2 = camera_to_world_y(&w->camera, w->ry1 < w->ry2 ? w->ry2 : w->ry1);

	if (w2 - w1 < (double)spawnregion_min_size ||
			h2 - h1 < (double)spawnregion_min_size) {
		fprintf(stderr, "seraphim: spawn region too small (%.0fx%.0f canvas px, "
				"needs %.0f)\n", w2 - w1, h2 - h1,
				(double)spawnregion_min_size);
		return false;
	}

	ox = (float)w1;
	oy = (float)h1;
	ow = (float)(w2 - w1);
	oh = (float)(h2 - h1);

	world_spawn(w, rect_of(w1, h1, w2 - w1, h2 - h1));
	return true;
}

static std::string window_title(int id)
{
	Window *win = win_or_null(id);
	return win ? win->title : std::string();
}

static std::string window_app_id(int id)
{
	Window *win = win_or_null(id);
	return win ? win->app_id : std::string();
}

static bool window_in_view(int id)
{
	Window *win = win_or_null(id);
	Rect r;
	double cx, cy;

	if (!win)
		return false;
	visible_rect(&r);
	cx = win->geom.x + win->geom.w / 2.0;
	cy = win->geom.y + win->geom.h / 2.0;
	return cx >= r.x && cx <= r.x + r.w && cy >= r.y && cy <= r.y + r.h;
}

static bool bookmark_save(int slot)
{
	World *w = world();
	return w && world_bookmark_save(w, slot);
}

static bool bookmark_goto(int slot)
{
	World *w = world();
	return w && world_bookmark_goto(w, slot);
}

static bool bookmark_exists(int slot)
{
	World *w = world();
	return w && slot >= 0 && slot < BOOKMARK_SLOTS && w->bookmarks[slot].set;
}

static int key_digit(unsigned keysym)
{
	if (keysym >= 0x31 && keysym <= 0x39)
		return (int)(keysym - 0x30);
	return -1;
}

static void script_spawn(const std::string &cmd)
{
	World *w = world();
	if (w)
		world_queue_spawn(w, cmd);
}

static void script_reload(void)
{
	World *w = world();
	if (w)
		w->reload_request = true;
}

static void script_quit(void)
{
	World *w = world();
	if (w)
		w->quit_request = true;
}

static void ipc_reply(const std::string &text)
{
	World *w = world();
	if (!w)
		return;
	w->ipc_out += text;
	w->ipc_out += "\n";
}

static void script_log(const std::string &msg)
{
	fprintf(stderr, "seraphim: %s\n", msg.c_str());
}

static void message_callback(const asSMessageInfo *msg, void *param)
{
	const char *type = "error";

	(void)param;
	if (msg->type == asMSGTYPE_INFORMATION)
		return;
	if (msg->type == asMSGTYPE_WARNING)
		type = "warning";
	fprintf(stderr, "seraphim: script %s (%d:%d): %s\n", type, msg->row,
			msg->col, msg->message);
}

#define REG(x) do { \
	if (engine->x < 0) { \
		error = "failed to register " #x; \
		return false; \
	} \
} while (0)

bool Script::register_api()
{
	asIScriptEngine *engine = this->engine;

	REG(RegisterGlobalProperty("const uint MODKEY", &cfg_modkey));
	REG(RegisterGlobalProperty("const uint MOD_ALT", &cfg_mod_alt));
	REG(RegisterGlobalProperty("const int BTN_LEFT", &cfg_btn_left));
	REG(RegisterGlobalProperty("const int BTN_MIDDLE", &cfg_btn_middle));
	REG(RegisterGlobalProperty("const int BTN_RIGHT", &cfg_btn_right));
	REG(RegisterGlobalProperty("const uint KEY_LEFT", &cfg_key_left));
	REG(RegisterGlobalProperty("const uint KEY_UP", &cfg_key_up));
	REG(RegisterGlobalProperty("const uint KEY_RIGHT", &cfg_key_right));
	REG(RegisterGlobalProperty("const uint KEY_DOWN", &cfg_key_down));
	REG(RegisterGlobalProperty("const uint KEY_HOME", &cfg_key_home));
	REG(RegisterGlobalProperty("const uint KEY_MINUS", &cfg_key_minus));
	REG(RegisterGlobalProperty("const uint KEY_EQUAL", &cfg_key_equal));
	REG(RegisterGlobalProperty("const uint KEY_0", &cfg_key_0));
	REG(RegisterGlobalProperty("const uint KEY_Q", &cfg_key_q));
	REG(RegisterGlobalProperty("const uint KEY_T", &cfg_key_t));
	REG(RegisterGlobalProperty("const uint KEY_W", &cfg_key_w));
	REG(RegisterGlobalProperty("const uint KEY_A", &cfg_key_a));
	REG(RegisterGlobalProperty("const uint KEY_S", &cfg_key_s));
	REG(RegisterGlobalProperty("const uint KEY_D", &cfg_key_d));
	REG(RegisterGlobalProperty("const uint KEY_Z", &cfg_key_z));
	REG(RegisterGlobalProperty("const uint KEY_TAB", &cfg_key_tab));
	REG(RegisterGlobalProperty("const uint KEY_L", &cfg_key_l));
	REG(RegisterGlobalProperty("const uint KEY_R", &cfg_key_r));
	REG(RegisterGlobalProperty("const uint MOD_CTRL", &cfg_mod_ctrl));
	REG(RegisterGlobalProperty("const uint MOD_SHIFT", &cfg_mod_shift));
	REG(RegisterGlobalProperty("const int spawn_button", &cfg_spawn_button));

	REG(RegisterGlobalProperty("const float magnet_snap_dist", &cfg_magnet_dist));
	REG(RegisterGlobalProperty("const float align_snap_dist", &cfg_align_dist));
	REG(RegisterGlobalProperty("const float window_gap", &cfg_window_gap));
	REG(RegisterGlobalProperty("const float zoom_min", &cfg_zoom_min));
	REG(RegisterGlobalProperty("const float zoom_max", &cfg_zoom_max));
	REG(RegisterGlobalProperty("const float zoom_step", &cfg_zoom_step));
	REG(RegisterGlobalProperty("const float canvas_pan_step", &cfg_pan_step));
	REG(RegisterGlobalProperty("const int min_window_w", &cfg_min_window_w));
	REG(RegisterGlobalProperty("const int min_window_h", &cfg_min_window_h));

	REG(RegisterGlobalFunction("float canvas_scale()", asFUNCTION(canvas_scale), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_set_scale(float)", asFUNCTION(canvas_set_scale), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_set_scale_at(float, float, float)", asFUNCTION(canvas_set_scale_at), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_zoom(float)", asFUNCTION(canvas_zoom), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_zoom_at(float, float, float)", asFUNCTION(canvas_zoom_at), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_pan_x()", asFUNCTION(canvas_pan_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_pan_y()", asFUNCTION(canvas_pan_y), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_set_pan(float, float)", asFUNCTION(canvas_set_pan), asCALL_CDECL));
	REG(RegisterGlobalFunction("void canvas_pan_by(float, float)", asFUNCTION(canvas_pan_by), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_to_world_x(float)", asFUNCTION(canvas_to_world_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_to_world_y(float)", asFUNCTION(canvas_to_world_y), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_to_screen_x(float)", asFUNCTION(canvas_to_screen_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float canvas_to_screen_y(float)", asFUNCTION(canvas_to_screen_y), asCALL_CDECL));

	REG(RegisterGlobalFunction("float view_x()", asFUNCTION(view_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float view_y()", asFUNCTION(view_y), asCALL_CDECL));
	REG(RegisterGlobalFunction("float view_w()", asFUNCTION(view_w), asCALL_CDECL));
	REG(RegisterGlobalFunction("float view_h()", asFUNCTION(view_h), asCALL_CDECL));
	REG(RegisterGlobalFunction("float view_center_x()", asFUNCTION(view_center_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float view_center_y()", asFUNCTION(view_center_y), asCALL_CDECL));

	REG(RegisterGlobalFunction("int window_count()", asFUNCTION(window_count), asCALL_CDECL));
	REG(RegisterGlobalFunction("int window_id_at(int)", asFUNCTION(window_id_at), asCALL_CDECL));
	REG(RegisterGlobalFunction("float window_x(int)", asFUNCTION(window_x), asCALL_CDECL));
	REG(RegisterGlobalFunction("float window_y(int)", asFUNCTION(window_y), asCALL_CDECL));
	REG(RegisterGlobalFunction("float window_w(int)", asFUNCTION(window_w), asCALL_CDECL));
	REG(RegisterGlobalFunction("float window_h(int)", asFUNCTION(window_h), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_set_geom(int, float, float, float, float)", asFUNCTION(window_set_geom), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_move(int, float, float)", asFUNCTION(window_move), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_focus(int)", asFUNCTION(window_focus), asCALL_CDECL));
	REG(RegisterGlobalFunction("int window_focused()", asFUNCTION(window_focused), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool window_is_focused(int)", asFUNCTION(window_is_focused), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_raise(int)", asFUNCTION(window_raise), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_set_urgent(int, bool)", asFUNCTION(window_set_urgent), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool window_urgent(int)", asFUNCTION(window_urgent), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_close(int)", asFUNCTION(window_close), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_set_resize_edges(int, int)", asFUNCTION(window_set_resize_edges), asCALL_CDECL));
	REG(RegisterGlobalFunction("void cursor_warp(float, float)", asFUNCTION(cursor_warp), asCALL_CDECL));
	REG(RegisterGlobalFunction("int window_at_point(float, float)", asFUNCTION(window_at_point), asCALL_CDECL));
	REG(RegisterGlobalFunction("int window_at_screen_point(float, float)", asFUNCTION(window_at_screen_point), asCALL_CDECL));
	REG(RegisterGlobalFunction("void window_spawn(float, float, float, float)", asFUNCTION(window_spawn), asCALL_CDECL));

	REG(RegisterGlobalFunction("void region_begin(float, float)", asFUNCTION(region_begin), asCALL_CDECL));
	REG(RegisterGlobalFunction("void region_update(float, float)", asFUNCTION(region_update), asCALL_CDECL));
	REG(RegisterGlobalFunction("void region_cancel()", asFUNCTION(region_cancel), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool region_active()", asFUNCTION(region_active), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool region_finish(float &out, float &out, float &out, float &out)", asFUNCTION(region_finish), asCALL_CDECL));

	REG(RegisterGlobalFunction("string window_title(int)", asFUNCTION(window_title), asCALL_CDECL));
	REG(RegisterGlobalFunction("string window_app_id(int)", asFUNCTION(window_app_id), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool window_in_view(int)", asFUNCTION(window_in_view), asCALL_CDECL));

	REG(RegisterGlobalFunction("bool bookmark_save(int)", asFUNCTION(bookmark_save), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool bookmark_goto(int)", asFUNCTION(bookmark_goto), asCALL_CDECL));
	REG(RegisterGlobalFunction("bool bookmark_exists(int)", asFUNCTION(bookmark_exists), asCALL_CDECL));
	REG(RegisterGlobalFunction("int key_digit(uint)", asFUNCTION(key_digit), asCALL_CDECL));

	REG(RegisterGlobalFunction("void spawn(const string &in)", asFUNCTION(script_spawn), asCALL_CDECL));
	REG(RegisterGlobalFunction("void reload_script()", asFUNCTION(script_reload), asCALL_CDECL));
	REG(RegisterGlobalFunction("void quit()", asFUNCTION(script_quit), asCALL_CDECL));
	REG(RegisterGlobalFunction("void ipc_reply(const string &in)", asFUNCTION(ipc_reply), asCALL_CDECL));
	REG(RegisterGlobalFunction("void log(const string &in)", asFUNCTION(script_log), asCALL_CDECL));

	return true;
}

#undef REG

bool Script::init(World *w)
{
	world = w;
	world_set_active(w);

	engine = asCreateScriptEngine();
	if (!engine) {
		error = "asCreateScriptEngine failed";
		return false;
	}
	engine->SetMessageCallback(asFUNCTION(message_callback), nullptr, asCALL_CDECL);

	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
	if (!register_api())
		return false;

	ctx = engine->CreateContext();
	if (!ctx) {
		error = "CreateContext failed";
		return false;
	}
	return true;
}

bool Script::load_source(const char *name, const char *code)
{
	if (!engine) {
		error = "engine is not up";
		return false;
	}
	module = engine->GetModule("seraphim", asGM_ALWAYS_CREATE);
	if (!module) {
		error = "GetModule failed";
		return false;
	}
	if (module->AddScriptSection(name, code) < 0) {
		error = "AddScriptSection failed";
		return false;
	}
	if (module->Build() < 0) {
		error = "the script did not compile";
		return false;
	}
	loaded = true;
	return resolve_entry_points();
}

bool Script::load_file(const std::string &path)
{
	std::ifstream in(path.c_str());
	std::stringstream buf;

	if (!in) {
		error = "cannot open " + path;
		return false;
	}
	buf << in.rdbuf();
	this->path = path;
	return load_source(path.c_str(), buf.str().c_str());
}

bool Script::resolve_entry_points()
{
	struct Entry {
		const char *decl;
		asIScriptFunction **slot;
		bool required;
		bool quiet;
	};
	Entry entries[] = {
		{ "void seraphim_init()", &fn_init, true, false },
		{ "void seraphim_pointer_motion(float sx, float sy, int hit, uint mods)", &fn_motion, false, false },
		{ "bool seraphim_pointer_button(int button, bool pressed, float sx, float sy, int hit, uint mods)", &fn_button, false, false },
		{ "void seraphim_pointer_axis(float delta, float sx, float sy, uint mods)", &fn_axis, false, false },
		{ "bool seraphim_key(uint keysym, uint mods, bool pressed)", &fn_key, false, false },
		{ "void seraphim_window_open(int id)", &fn_window_open, false, false },
		{ "void seraphim_window_close(int id)", &fn_window_close, false, false },
		{ "void seraphim_window_changed(int id)", &fn_window_changed, false, true },
		{ "void seraphim_reload()", &fn_reload, false, true },
	};

	for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
		*entries[i].slot = module->GetFunctionByDecl(entries[i].decl);
		if (!*entries[i].slot) {
			if (entries[i].required) {
				error = std::string("the script has no ") + entries[i].decl;
				return false;
			}
			if (!entries[i].quiet)
				fprintf(stderr, "seraphim: script has no %s, that part of the "
					"desk will not answer\n", entries[i].decl);
		}
	}
	return true;
}

void Script::call_window_changed(int id)
{
	if (!fn_window_changed || ctx->Prepare(fn_window_changed) < 0)
		return;
	ctx->SetArgDWord(0, (asDWORD)id);
	execute("seraphim_window_changed");
}

void Script::call_reload()
{
	if (!fn_reload || ctx->Prepare(fn_reload) < 0)
		return;
	execute("seraphim_reload");
}

bool Script::reload()
{
	std::ifstream in(path.c_str());
	std::stringstream buf;
	std::string code;
	char modname[48];
	asIScriptModule *next, *old;

	if (!engine || path.empty()) {
		error = "reload: no script file to reload";
		return false;
	}
	if (!in) {
		error = "reload: cannot open " + path;
		return false;
	}
	buf << in.rdbuf();
	code = buf.str();

	snprintf(modname, sizeof(modname), "seraphim_r%d", ++generation);
	next = engine->GetModule(modname, asGM_ALWAYS_CREATE);
	if (!next || next->AddScriptSection(path.c_str(), code.c_str()) < 0) {
		error = "reload: could not start a new module";
		return false;
	}
	if (next->Build() < 0) {
		next->Discard();
		error = "reload: the script did not compile, keeping the running one";
		fprintf(stderr, "seraphim: %s\n", error.c_str());
		return false;
	}
	if (!next->GetFunctionByDecl("void seraphim_init()")) {
		next->Discard();
		error = "reload: the new script has no void seraphim_init()";
		fprintf(stderr, "seraphim: %s\n", error.c_str());
		return false;
	}

	ctx->Unprepare();
	old = module;
	module = next;
	resolve_entry_points();
	if (old)
		old->Discard();
	loaded = true;
	fprintf(stderr, "seraphim: reloaded %s\n", path.c_str());
	call_reload();
	return true;
}

bool Script::call_named(const std::string &name, const std::string &arg,
		std::string *why)
{
	asIScriptFunction *fn;
	std::string with_arg, without;
	size_t i;

	if (!module || !ctx) {
		*why = "no script is loaded";
		return false;
	}
	if (name.compare(0, 4, "ipc_") != 0 || name.size() > 64) {
		*why = "only script functions named ipc_* can be called";
		return false;
	}
	for (i = 0; i < name.size(); i++) {
		char c = name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_')) {
			*why = "bad function name";
			return false;
		}
	}

	with_arg = "void " + name + "(const string &in)";
	without = "void " + name + "()";
	fn = module->GetFunctionByDecl(with_arg.c_str());
	if (fn) {
		std::string a = arg;
		if (ctx->Prepare(fn) < 0) {
			*why = "could not prepare " + name;
			return false;
		}
		ctx->SetArgObject(0, &a);
		if (!execute(name.c_str())) {
			*why = error;
			return false;
		}
		return true;
	}
	fn = module->GetFunctionByDecl(without.c_str());
	if (!fn) {
		*why = "the script has no void " + name + "() or void " + name +
				"(const string &in)";
		return false;
	}
	if (ctx->Prepare(fn) < 0) {
		*why = "could not prepare " + name;
		return false;
	}
	if (!execute(name.c_str())) {
		*why = error;
		return false;
	}
	return true;
}

void Script::shutdown()
{
	if (ctx)
		ctx->Release();
	if (engine)
		engine->ShutDownAndRelease();
	ctx = nullptr;
	engine = nullptr;
	module = nullptr;
	loaded = false;
}

bool Script::execute(const char *what)
{
	int r;

	if (!ctx)
		return false;
	r = ctx->Execute();
	if (r == asEXECUTION_FINISHED)
		return true;

	if (r == asEXECUTION_EXCEPTION) {
		const char *msg = ctx->GetExceptionString();
		const char *fn = ctx->GetExceptionFunction() ? ctx->GetExceptionFunction()->GetName() : "?";
		error = std::string(fn) + "(): " + (msg ? msg : "?");
	} else {
		error = std::string(what) + ": script did not finish";
	}
	fprintf(stderr, "seraphim: %s\n", error.c_str());
	ctx->Abort();
	return false;
}

bool Script::call_init()
{
	if (!fn_init)
		return false;
	if (ctx->Prepare(fn_init) < 0)
		return false;
	return execute("seraphim_init");
}

void Script::call_motion(float sx, float sy, int hit, unsigned mods)
{
	if (!fn_motion || ctx->Prepare(fn_motion) < 0)
		return;
	ctx->SetArgFloat(0, sx);
	ctx->SetArgFloat(1, sy);
	ctx->SetArgDWord(2, (asDWORD)hit);
	ctx->SetArgDWord(3, (asDWORD)mods);
	execute("seraphim_pointer_motion");
}

bool Script::call_button(int button, bool pressed, float sx, float sy, int hit, unsigned mods)
{
	if (!fn_button || ctx->Prepare(fn_button) < 0)
		return false;
	ctx->SetArgDWord(0, (asDWORD)button);
	ctx->SetArgByte(1, pressed ? 1 : 0);
	ctx->SetArgFloat(2, sx);
	ctx->SetArgFloat(3, sy);
	ctx->SetArgDWord(4, (asDWORD)hit);
	ctx->SetArgDWord(5, (asDWORD)mods);
	if (!execute("seraphim_pointer_button"))
		return false;
	return ctx->GetReturnByte() != 0;
}

void Script::call_axis(float delta, float sx, float sy, unsigned mods)
{
	if (!fn_axis || ctx->Prepare(fn_axis) < 0)
		return;
	ctx->SetArgFloat(0, delta);
	ctx->SetArgFloat(1, sx);
	ctx->SetArgFloat(2, sy);
	ctx->SetArgDWord(3, (asDWORD)mods);
	execute("seraphim_pointer_axis");
}

bool Script::call_key(unsigned keysym, unsigned mods, bool pressed)
{
	if (!fn_key || ctx->Prepare(fn_key) < 0)
		return false;
	ctx->SetArgDWord(0, (asDWORD)keysym);
	ctx->SetArgDWord(1, (asDWORD)mods);
	ctx->SetArgByte(2, pressed ? 1 : 0);
	if (!execute("seraphim_key"))
		return false;
	return ctx->GetReturnByte() != 0;
}

void Script::call_window_open(int id)
{
	if (!fn_window_open || ctx->Prepare(fn_window_open) < 0)
		return;
	ctx->SetArgDWord(0, (asDWORD)id);
	execute("seraphim_window_open");
}

void Script::call_window_close(int id)
{
	if (!fn_window_close || ctx->Prepare(fn_window_close) < 0)
		return;
	ctx->SetArgDWord(0, (asDWORD)id);
	execute("seraphim_window_close");
}

static bool readable(const std::string &path)
{
	std::ifstream in(path.c_str());
	return in.good();
}

std::string Script::find_script(const char *argv0)
{
	const char *env = getenv("SERAPHIM_SCRIPT");
	const char *home = getenv("HOME");
	std::vector<std::string> places;

	if (env && *env)
		places.push_back(env);
	if (home && *home) {
		places.push_back(std::string(home) + "/.config/seraphim/seraphim.as");
		places.push_back(std::string(home) + "/.config/seraphim/seraphim.as.zip");
	}
	places.push_back("/usr/local/share/seraphim/seraphim.as");
	places.push_back("/usr/share/seraphim/seraphim.as");
	places.push_back("scripts/seraphim.as");
	if (argv0) {
		std::string dir(argv0);
		size_t slash = dir.find_last_of('/');
		if (slash != std::string::npos) {
			dir = dir.substr(0, slash);
			places.push_back(dir + "/scripts/seraphim.as");
			places.push_back(dir + "/../share/seraphim/seraphim.as");
		}
	}

	for (size_t i = 0; i < places.size(); i++) {
		if (readable(places[i]))
			return places[i];
	}
	return std::string();
}
