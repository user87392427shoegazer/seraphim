#include "canvas.h"
#include "borders.h"
#include "ipc.h"
#include "script.h"
#include "world.h"

#include "config_animation.h"
#include "config_appearance.h"
#include "config_input.h"
#include "config_keys.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { \
		failures++; \
		printf("  FAIL line %d: %s\n", __LINE__, #cond); \
	} \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
	double _a = (a), _b = (b); \
	checks++; \
	if (std::fabs(_a - _b) > (eps)) { \
		failures++; \
		printf("  FAIL line %d: %s == %.4f, expected %.4f\n", \
				__LINE__, #a, _a, _b); \
	} \
} while (0)

static Rect mkrect(double x, double y, double w, double h)
{
	Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}

struct Host {
	World world;
	Script script;
};

static int hit_at(World *w, double sx, double sy)
{
	return w->window_at_point(camera_to_world_x(&w->camera, sx),
			camera_to_world_y(&w->camera, sy));
}

static void host_pump(Host *h)
{
	Rect geo;
	int id;

	if (world_take_spawn_request(&h->world) &&
			world_take_pending_geometry(&h->world, &geo)) {
		id = h->world.add(geo);
		h->script.call_window_open(id);
	}
	while ((id = world_take_close(&h->world)) != 0)
		h->world.remove(id);
}

static bool press(Host *h, int button, double sx, double sy, unsigned mods)
{
	return h->script.call_button(button, true, (float)sx, (float)sy,
			hit_at(&h->world, sx, sy), mods);
}

static void motion(Host *h, double sx, double sy, unsigned mods)
{
	h->script.call_motion((float)sx, (float)sy, hit_at(&h->world, sx, sy), mods);
	host_pump(h);
}

static bool release(Host *h, int button, double sx, double sy, unsigned mods)
{
	bool consumed = h->script.call_button(button, false, (float)sx, (float)sy,
			hit_at(&h->world, sx, sy), mods);
	host_pump(h);
	return consumed;
}

static void wheel(Host *h, double delta, double sx, double sy, unsigned mods)
{
	h->script.call_axis((float)delta, (float)sx, (float)sy, mods);
	host_pump(h);
}

static bool key(Host *h, unsigned keysym, unsigned mods)
{
	bool consumed = h->script.call_key(keysym, mods, true);
	host_pump(h);
	return consumed;
}

static void test_camera(void)
{
	Camera c;
	Rect vis;

	printf("-- camera\n");
	camera_init(&c, ZOOMMIN, ZOOMMAX);
	CHECK_NEAR(c.scale, 1.0, 1e-9);

	camera_pan_by(&c, 100.0, -40.0);
	CHECK_NEAR(camera_to_screen_x(&c, 10.0), 110.0, 1e-9);
	CHECK_NEAR(camera_to_screen_y(&c, 10.0), -30.0, 1e-9);
	CHECK_NEAR(camera_to_world_x(&c, 110.0), 10.0, 1e-9);
	CHECK_NEAR(camera_to_world_y(&c, -30.0), 10.0, 1e-9);

	camera_zoom_at(&c, -0.25, 300.0, 200.0);
	CHECK_NEAR(c.scale, 0.75, 1e-9);
	CHECK_NEAR(camera_to_screen_x(&c, camera_to_world_x(&c, 300.0)), 300.0, 1e-9);
	CHECK_NEAR(camera_to_screen_y(&c, camera_to_world_y(&c, 200.0)), 200.0, 1e-9);

	camera_set_scale_at(&c, 9.0, 0.0, 0.0);
	CHECK_NEAR(c.scale, ZOOMMAX, 1e-9);
	camera_set_scale_at(&c, 0.01, 0.0, 0.0);
	CHECK_NEAR(c.scale, ZOOMMIN, 1e-9);

	camera_visible_rect(&c, 1600.0, 900.0, &vis);
	CHECK_NEAR(vis.w, 1600.0 / ZOOMMIN, 1e-6);
	CHECK_NEAR(vis.h, 900.0 / ZOOMMIN, 1e-6);
}

static void test_borders(void)
{
	Rect geom = mkrect(100.0, 100.0, 400.0, 300.0);
	BorderRects br;

	printf("-- double borders\n");
	border_layout(&geom, 2, 2, &br);

	CHECK_NEAR(br.inner[BORDER_TOP].x, 98.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_TOP].y, 98.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_TOP].w, 404.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_TOP].h, 2.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_LEFT].x, 98.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_LEFT].y, 100.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_LEFT].w, 2.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_LEFT].h, 300.0, 1e-9);

	CHECK_NEAR(br.outer[BORDER_TOP].x, 96.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].y, 96.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].w, 408.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].h, 2.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_RIGHT].x, 502.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_RIGHT].y, 98.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_RIGHT].w, 2.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_RIGHT].h, 304.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_BOTTOM].y, 402.0, 1e-9);

	border_layout(&geom, 0, 0, &br);
	CHECK_NEAR(br.inner[BORDER_TOP].x, 100.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_TOP].w, 400.0, 1e-9);
	CHECK_NEAR(br.inner[BORDER_TOP].h, 0.0, 1e-9);

	border_layout(&geom, 4, 1, &br);
	CHECK_NEAR(br.inner[BORDER_TOP].h, 4.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].x, 95.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].y, 95.0, 1e-9);
	CHECK_NEAR(br.outer[BORDER_TOP].h, 1.0, 1e-9);
}

static void test_desk(Host *h)
{
	World *w = &h->world;
	int a, b, id;

	printf("-- hit testing and focus\n");
	a = w->add(mkrect(100, 100, 400, 300));
	b = w->add(mkrect(600, 100, 400, 300));
	CHECK(w->window_at_point(150, 150) == a);
	CHECK(w->window_at_point(650, 150) == b);
	CHECK(w->window_at_point(1400, 800) == 0);
	CHECK(hit_at(w, 300, 250) == a);

	CHECK(press(h, BTN_LEFT, 300, 250, 0) == false);
	CHECK(w->focused_id == a);
	release(h, BTN_LEFT, 300, 250, 0);

	printf("-- dragging a window across the canvas\n");
	press(h, BTN_LEFT, 300, 250, MODKEY);
	motion(h, 337, 273, MODKEY);
	release(h, BTN_LEFT, 337, 273, MODKEY);
	CHECK_NEAR(w->find(a)->geom.x, 137.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.y, 123.0, 1e-3);

	printf("-- magnet snapping to a neighbour, then lining up with it\n");
	press(h, BTN_LEFT, 300, 250, MODKEY);

	motion(h, 357, 250, MODKEY);
	release(h, BTN_LEFT, 357, 250, MODKEY);
	CHECK_NEAR(w->find(a)->geom.x, 200.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.y, 100.0, 1e-3);

	printf("-- snapping to the screen edge\n");
	press(h, BTN_LEFT, 300, 250, MODKEY);
	motion(h, 106, 250, MODKEY);
	release(h, BTN_LEFT, 106, 250, MODKEY);
	CHECK_NEAR(w->find(a)->geom.x, 0.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.y, 100.0, 1e-3);

	printf("-- panning the canvas by dragging the empty canvas\n");
	CHECK(press(h, BTN_LEFT, 1200, 700, 0) == true);
	motion(h, 1300, 660, 0);
	release(h, BTN_LEFT, 1300, 660, 0);
	CHECK_NEAR(w->camera.pan_x, 100.0, 1e-3);
	CHECK_NEAR(w->camera.pan_y, -40.0, 1e-3);
	CHECK_NEAR(w->camera.scale, 1.0, 1e-3);
	CHECK_NEAR(camera_to_screen_x(&w->camera, 0.0), 100.0, 1e-3);
	CHECK_NEAR(camera_to_screen_y(&w->camera, 100.0), 60.0, 1e-3);

	printf("-- zooming with the wheel, anchored at the pointer\n");
	{
		double wx = camera_to_world_x(&w->camera, 800.0);
		double wy = camera_to_world_y(&w->camera, 450.0);
		wheel(h, -1.0, 800, 450, 0);
		CHECK_NEAR(w->camera.scale, 1.0 - (double)ZOOMSTEP, 1e-3);
		CHECK_NEAR(camera_to_world_x(&w->camera, 800.0), wx, 1e-3);
		CHECK_NEAR(camera_to_world_y(&w->camera, 450.0), wy, 1e-3);

		CHECK_NEAR(w->camera.pan_x, 170.0, 1e-3);
		CHECK_NEAR(w->camera.pan_y, 9.0, 1e-3);

		for (int i = 0; i < 30; i++)
			wheel(h, -1.0, 800, 450, 0);
		CHECK_NEAR(w->camera.scale, ZOOMMIN, 1e-6);
		for (int i = 0; i < 30; i++)
			wheel(h, 1.0, 800, 450, 0);
		CHECK_NEAR(w->camera.scale, ZOOMMAX, 1e-6);
	}

	printf("-- keyboard camera controls\n");
	{
		double before = w->camera.pan_x;
		CHECK(key(h, SERAPHIM_KEY_LEFT, MODKEY) == true);
		CHECK_NEAR(w->camera.pan_x, before + CANVAS_PAN_STEP, 1e-3);
	}
	CHECK(key(h, SERAPHIM_KEY_0, MODKEY) == true);
	CHECK_NEAR(w->camera.scale, 1.0, 1e-6);
	CHECK_NEAR(w->camera.pan_x, 0.0, 1e-6);
	CHECK_NEAR(w->camera.pan_y, 0.0, 1e-6);

	CHECK(key(h, 0x0061, 0) == false);

	printf("-- resizing with Super+right drag\n");

	CHECK(press(h, BTN_RIGHT, 400, 400, MODKEY) == true);
	motion(h, 500, 450, MODKEY);
	release(h, BTN_RIGHT, 500, 450, MODKEY);
	CHECK_NEAR(w->find(a)->geom.x, 0.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.y, 100.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.w, 500.0, 1e-3);
	CHECK_NEAR(w->find(a)->geom.h, 350.0, 1e-3);

	press(h, BTN_RIGHT, 500, 450, MODKEY);
	motion(h, -400, -400, MODKEY);
	release(h, BTN_RIGHT, -400, -400, MODKEY);
	CHECK_NEAR(w->find(a)->geom.w, (double)min_window_w, 1e-3);
	CHECK_NEAR(w->find(a)->geom.h, (double)min_window_h, 1e-3);
	CHECK_NEAR(w->find(a)->geom.x, 0.0, 1e-3);

	printf("-- drag-to-spawn region\n");
	CHECK(press(h, BTN_RIGHT, 1200, 600, 0) == true);
	CHECK(w->region_active == true);
	motion(h, 1550, 850, 0);
	release(h, BTN_RIGHT, 1550, 850, 0);
	CHECK(w->count() == 3);
	id = w->windows.back().id;
	CHECK_NEAR(w->find(id)->geom.x, 1200.0, 1e-3);
	CHECK_NEAR(w->find(id)->geom.y, 600.0, 1e-3);
	CHECK_NEAR(w->find(id)->geom.w, 350.0, 1e-3);
	CHECK_NEAR(w->find(id)->geom.h, 250.0, 1e-3);
	CHECK(w->focused_id == id);

	printf("-- a spawn region follows the zoom level\n");

	for (int i = 0; i < 30; i++)
		wheel(h, -1.0, 300, 800, 0);
	CHECK_NEAR(w->camera.scale, ZOOMMIN, 1e-6);
	CHECK(press(h, BTN_RIGHT, 300, 800, 0) == true);
	motion(h, 400, 900, 0);
	release(h, BTN_RIGHT, 400, 900, 0);
	CHECK(w->count() == 4);
	id = w->windows.back().id;

	CHECK_NEAR(w->find(id)->geom.w, 100.0 / ZOOMMIN, 0.5);
	CHECK_NEAR(w->find(id)->geom.h, 100.0 / ZOOMMIN, 0.5);
	CHECK(key(h, SERAPHIM_KEY_0, MODKEY) == true);

	printf("-- right click with no drag opens a window at the default size\n");
	CHECK(press(h, BTN_RIGHT, 1400, 300, 0) == true);
	release(h, BTN_RIGHT, 1400, 300, 0);
	CHECK(w->count() == 5);
	id = w->windows.back().id;
	CHECK_NEAR(w->find(id)->geom.w, 1600.0 * new_window_w_frac, 1e-3);
	CHECK_NEAR(w->find(id)->geom.h, 900.0 * new_window_h_frac, 1e-3);
	CHECK(w->focused_id == id);

	printf("-- closing a window through the desk\n");
	CHECK(key(h, SERAPHIM_KEY_Q, MODKEY) == true);
	CHECK(w->count() == 4);
	CHECK(w->focused_id != id);
}

static Host *fresh_host(const char *path, const char *source = nullptr)
{
	Host *h = new Host();
	bool ok;

	h->world.view_w = 1600.0;
	h->world.view_h = 900.0;
	ok = h->script.init(&h->world) &&
			(source ? h->script.load_source("test", source) : h->script.load_file(path)) &&
			h->script.call_init();
	if (!ok) {
		printf("  could not start a host: %s\n", h->script.error.c_str());
		failures++;
	}
	return h;
}

static void drop_host(Host *h)
{
	h->script.shutdown();
	delete h;
}

static void write_file(const std::string &path, const std::string &text)
{
	std::ofstream out(path.c_str());
	out << text;
}

static std::string read_file(const std::string &path)
{
	std::ifstream in(path.c_str());
	std::stringstream buf;
	buf << in.rdbuf();
	return buf.str();
}

static bool starts_with(const std::string &s, const char *prefix)
{
	return s.compare(0, strlen(prefix), prefix) == 0;
}

static bool contains(const std::string &s, const char *part)
{
	return s.find(part) != std::string::npos;
}

static void test_bookmarks(const char *path)
{
	Host *h = fresh_host(path);
	unsigned save = MODKEY | SERAPHIM_MOD_CTRL;

	printf("-- bookmarks\n");

	h->world.camera.pan_x = -300.0;
	h->world.camera.pan_y = 120.0;
	h->world.camera.scale = 0.5;
	CHECK(key(h, 0x31, save));
	CHECK(h->world.bookmarks[1].set);

	h->world.camera.pan_x = 0.0;
	h->world.camera.pan_y = 0.0;
	h->world.camera.scale = 1.0;
	CHECK(key(h, 0x31, MODKEY));
	CHECK_NEAR(h->world.camera.pan_x, -300.0, 1e-9);
	CHECK_NEAR(h->world.camera.pan_y, 120.0, 1e-9);
	CHECK_NEAR(h->world.camera.scale, 0.5, 1e-9);

	h->world.camera.pan_x = 5.0;
	CHECK(key(h, 0x32, MODKEY));
	CHECK_NEAR(h->world.camera.pan_x, 5.0, 1e-9);

	CHECK(!key(h, 0x31, 0));

	drop_host(h);
}

static Rect geom_of(Host *h, int id)
{
	Window *w = h->world.find(id);
	return w ? w->geom : mkrect(0, 0, 0, 0);
}

static bool overlaps(const Rect &a, const Rect &b)
{
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void test_layouts(const char *path)
{
	Host *h = fresh_host(path);
	int ids[4], far_id, i, j;
	std::string out;

	printf("-- layouts\n");

	for (i = 0; i < 4; i++)
		ids[i] = h->world.add(mkrect(100.0 + 90.0 * i, 100.0 + 40.0 * i, 300, 200));
	far_id = h->world.add(mkrect(9000, 9000, 300, 200));

	CHECK(key(h, SERAPHIM_KEY_L, MODKEY));
	for (i = 0; i < 4; i++) {
		Rect r = geom_of(h, ids[i]);

		CHECK(r.x >= 0.0 && r.y >= 0.0 && r.x + r.w <= 1600.0 && r.y + r.h <= 900.0);
		for (j = i + 1; j < 4; j++)
			CHECK(!overlaps(r, geom_of(h, ids[j])));
	}
	CHECK_NEAR(geom_of(h, ids[0]).w, geom_of(h, ids[3]).w, 1e-3);
	CHECK(geom_of(h, ids[1]).x > geom_of(h, ids[0]).x);
	CHECK(geom_of(h, ids[2]).y > geom_of(h, ids[0]).y);

	CHECK_NEAR(geom_of(h, far_id).x, 9000.0, 1e-9);

	CHECK(key(h, SERAPHIM_KEY_L, MODKEY | SERAPHIM_MOD_CTRL));
	for (i = 1; i < 4; i++) {
		CHECK_NEAR(geom_of(h, ids[i]).y, geom_of(h, ids[0]).y, 1e-3);
		CHECK(geom_of(h, ids[i]).x > geom_of(h, ids[i - 1]).x);
	}
	CHECK_NEAR(geom_of(h, ids[0]).h, 900.0 - 48.0, 1e-3);

	out = ipc_execute(&h->world, &h->script, "layout master");
	CHECK(out == "ok\n");
	CHECK_NEAR(geom_of(h, ids[0]).h, 900.0 - 48.0, 1e-3);
	CHECK(geom_of(h, ids[1]).h < geom_of(h, ids[0]).h);

	out = ipc_execute(&h->world, &h->script, "layout nonsense");
	CHECK(starts_with(out, "error:"));

	out = ipc_execute(&h->world, &h->script, "layouts");
	CHECK(contains(out, "master *"));
	CHECK(contains(out, "grid"));
	CHECK(contains(out, "cascade"));

	drop_host(h);
}

static void test_rules(const char *path)
{
	std::string src = read_file(path);
	const char *example = "register_layout(\"cascade\", layout_cascade);";
	size_t at = src.find(example);
	Host *h;
	int video, term;

	printf("-- window rules and window info\n");

	CHECK(at != std::string::npos);
	if (at == std::string::npos)
		return;
	src.insert(at + strlen(example), "\nrule_size(\"mpv\", 960.0f, 540.0f);");
	write_file("/tmp/seraphim-test-rules.as", src);

	h = fresh_host("/tmp/seraphim-test-rules.as");

	video = h->world.add(mkrect(100, 100, 640, 480));
	h->world.find(video)->app_id = "mpv";
	h->world.find(video)->title = "a video";
	h->script.call_window_open(video);
	CHECK_NEAR(geom_of(h, video).w, 960.0, 1e-3);
	CHECK_NEAR(geom_of(h, video).h, 540.0, 1e-3);
	CHECK_NEAR(geom_of(h, video).x, 100.0, 1e-3);

	term = h->world.add(mkrect(100, 100, 640, 480));
	h->world.find(term)->app_id = "foot";
	h->script.call_window_open(term);
	CHECK_NEAR(geom_of(h, term).w, 640.0, 1e-3);

	drop_host(h);
}

static void test_ipc(const char *path)
{
	Host *h = fresh_host(path);
	World *w = &h->world;
	Script *s = &h->script;
	std::string out;
	int a, b;

	printf("-- ipc\n");

	CHECK(ipc_execute(w, s, "ping") == "pong\n");
	CHECK(contains(ipc_execute(w, s, "help"), "windows"));
	CHECK(starts_with(ipc_execute(w, s, "frobnicate"), "error:"));
	CHECK(starts_with(ipc_execute(w, s, ""), "error:"));

	CHECK(ipc_execute(w, s, "windows") == "ok\n");
	a = w->add(mkrect(10, 20, 300, 200));
	b = w->add(mkrect(400, 20, 300, 200));
	w->find(a)->app_id = "foot";
	w->find(a)->title = "a\ttitle";
	w->focus(a);
	out = ipc_execute(w, s, "windows");
	CHECK(contains(out, "10 20 300 200 1\tfoot\ta title"));
	CHECK(contains(out, "400 20 300 200 0\t\t"));

	CHECK(ipc_execute(w, s, "focused") == "1\n");
	CHECK(ipc_execute(w, s, "focus 2") == "ok\n");
	CHECK(w->focused_id == b);
	CHECK(starts_with(ipc_execute(w, s, "focus 99"), "error:"));
	CHECK(starts_with(ipc_execute(w, s, "focus banana"), "error:"));

	CHECK(ipc_execute(w, s, "move 1 50 60") == "ok\n");
	CHECK_NEAR(w->find(a)->geom.x, 50.0, 1e-9);
	CHECK(ipc_execute(w, s, "geom 1 0 0 500 400") == "ok\n");
	CHECK_NEAR(w->find(a)->geom.w, 500.0, 1e-9);
	ipc_execute(w, s, "geom 1 0 0 1 1");
	CHECK_NEAR(w->find(a)->geom.w, (double)min_window_w, 1e-9);

	CHECK(contains(ipc_execute(w, s, "camera"), "scale 1.0000"));
	CHECK(contains(ipc_execute(w, s, "zoom 0.5"), "scale 0.5000"));
	CHECK(starts_with(ipc_execute(w, s, "zoom -3"), "error:"));
	CHECK(ipc_execute(w, s, "pan 10 20") == "ok\n");
	CHECK(contains(ipc_execute(w, s, "view"), " "));

	CHECK(ipc_execute(w, s, "bookmark save 3") == "ok\n");
	CHECK(contains(ipc_execute(w, s, "bookmark list"), "3: scale 0.5000"));
	w->camera.scale = 1.0;
	CHECK(ipc_execute(w, s, "bookmark goto 3") == "ok\n");
	CHECK_NEAR(w->camera.scale, 0.5, 1e-9);
	CHECK(starts_with(ipc_execute(w, s, "bookmark goto 4"), "error:"));
	CHECK(starts_with(ipc_execute(w, s, "bookmark save 12"), "error:"));

	CHECK(ipc_execute(w, s, "close 2") == "ok\n");
	CHECK(world_take_close(w) == b);

	CHECK(ipc_execute(w, s, "spawn") == "ok\n");
	CHECK(world_take_spawn_request(w));
	CHECK(ipc_execute(w, s, "spawn foot -e htop") == "ok\n");
	{
		std::string cmd;
		CHECK(world_take_queued_spawn(w, &cmd));
		CHECK(cmd == "foot -e htop");
	}

	CHECK(ipc_execute(w, s, "call ipc_home") == "ok\n");
	CHECK_NEAR(w->camera.scale, 1.0, 1e-9);
	CHECK(starts_with(ipc_execute(w, s, "call seraphim_key"), "error:"));
	CHECK(starts_with(ipc_execute(w, s, "call ipc_does_not_exist"), "error:"));
	CHECK(starts_with(ipc_execute(w, s, "call"), "error:"));

	CHECK(!w->quit_request);
	CHECK(ipc_execute(w, s, "quit") == "ok\n");
	CHECK(w->quit_request);

	drop_host(h);
}

static void test_reload(void)
{
	const char *file = "/tmp/seraphim-reload-test.as";
	Host *h;
	std::string out;

	printf("-- hot reload\n");

	write_file(file,
		"int calls = 0;\n"
		"void seraphim_init() { }\n"
		"void ipc_who() { calls++; ipc_reply(\"one \" + calls); }\n"
		"void ipc_echo(const string &in arg) { ipc_reply(\"you said: \" + arg); }\n");
	h = fresh_host(file);

	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "one 1\n");
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "one 2\n");
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_echo hello there") == "you said: hello there\n");

	h->world.camera.pan_x = -40.0;
	h->world.camera.scale = 0.7;
	world_bookmark_save(&h->world, 5);
	int id = h->world.add(mkrect(1, 2, 300, 200));

	write_file(file,
		"int calls = 0;\n"
		"void seraphim_init() { canvas_set_pan(999.0f, 999.0f); }\n"
		"void seraphim_reload() { log(\"reloaded\"); }\n"
		"void ipc_who() { calls++; ipc_reply(\"two \" + calls); }\n");
	CHECK(ipc_execute(&h->world, &h->script, "reload") == "ok\n");
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "two 1\n");
	CHECK_NEAR(h->world.camera.pan_x, -40.0, 1e-9);
	CHECK_NEAR(h->world.camera.scale, 0.7, 1e-9);
	CHECK(h->world.bookmarks[5].set);
	CHECK(h->world.find(id) != nullptr);
	CHECK(starts_with(ipc_execute(&h->world, &h->script, "call ipc_echo x"), "error:"));

	write_file(file, "void seraphim_init() { this is not angelscript }\n");
	out = ipc_execute(&h->world, &h->script, "reload");
	CHECK(starts_with(out, "error:"));
	CHECK(contains(out, "did not compile"));
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "two 2\n");

	write_file(file, "void ipc_who() { ipc_reply(\"three\"); }\n");
	out = ipc_execute(&h->world, &h->script, "reload");
	CHECK(starts_with(out, "error:"));
	CHECK(contains(out, "seraphim_init"));
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "two 3\n");

	remove(file);
	CHECK(starts_with(ipc_execute(&h->world, &h->script, "reload"), "error:"));
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_who") == "two 4\n");

	write_file(file, "void seraphim_init() { }\nvoid ipc_ask() { reload_script(); }\n");
	CHECK(h->script.reload());
	CHECK(!h->world.reload_request);
	CHECK(ipc_execute(&h->world, &h->script, "call ipc_ask") == "ok\n");
	CHECK(h->world.reload_request);

	drop_host(h);
	remove(file);
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "scripts/seraphim.as";

	printf("seraphim tests\n");

	test_camera();
	test_borders();

	Host h;
	h.world.view_w = 1600.0;
	h.world.view_h = 900.0;

	if (!h.script.init(&h.world)) {
		printf("engine failed: %s\n", h.script.error.c_str());
		return 1;
	}
	if (!h.script.load_file(path)) {
		printf("script failed: %s\n", h.script.error.c_str());
		return 1;
	}
	if (!h.script.call_init()) {
		printf("seraphim_init failed: %s\n", h.script.error.c_str());
		return 1;
	}

	CHECK_NEAR(h.world.camera.scale, 1.0, 1e-9);
	CHECK_NEAR(h.world.camera.pan_x, 0.0, 1e-9);

	test_desk(&h);
	test_bookmarks(path);
	test_layouts(path);
	test_rules(path);
	test_ipc(path);
	test_reload();

	printf("\n%d checks, %d failures\n", checks, failures);
	h.script.shutdown();
	return failures ? 1 : 0;
}
