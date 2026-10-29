#define WLR_USE_UNSTABLE

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "borders.h"
#include "canvas.h"
#include "ipc.h"
#include "script.h"
#include "world.h"
#include "config.h"

extern "C" {
#include <wayland-server-core.h>

#include "wlr_cxx.h"

#include <wlr/config.h>

#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#define delete delete_
#include <wlr/types/wlr_input_method_v2.h>
#undef delete

#define namespace namespace_
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#define SERAPHIM_XWAYLAND 0

#include <wlr/util/log.h>

#include <xkbcommon/xkbcommon.h>
}

enum {
	LAYER_BACKGROUND,
	LAYER_LS_BACKGROUND,
	LAYER_LS_BOTTOM,
	LAYER_WINDOWS,
	LAYER_LS_TOP,
	LAYER_REGION,
	LAYER_LS_OVERLAY,
	LAYER_COUNT
};

struct Seraphim;

enum WindowKind {
	WIN_XDG,
	WIN_X11
};

struct ApplyKey {
	double x, y, w, h, scale, pan_x, pan_y;
	int shown_w, shown_h, edges, state;
};

struct SeraphimWindow {
	struct Seraphim *server;
	int id;
	enum WindowKind kind;
	struct wlr_xdg_toplevel *toplevel;
#if SERAPHIM_XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif
	struct wlr_scene_tree *tree;
	struct wlr_foreign_toplevel_handle_v1 *ftl;
	struct wlr_xdg_toplevel_decoration_v1 *deco;
	struct wlr_scene_rect *inner[4];
	struct wlr_scene_rect *outer[4];
	int want_w, want_h;
	uint32_t want_serial;
	int shown_w, shown_h;
	struct ApplyKey last;
	bool applied;
	bool sized;
	bool mapped;
	bool override_redirect;
	struct wl_listener map, unmap, commit, destroy;
	struct wl_listener xdg_title, xdg_app_id;
	struct wl_listener deco_request_mode, deco_destroy;
	struct wl_listener x_request_configure, x_title, x_class;
	struct wl_listener x_associate, x_dissociate;
	bool surface_hooks;
	struct wl_list link;
};

struct SeraphimLayer {
	struct Seraphim *server;
	struct wlr_layer_surface_v1 *layer_surface;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_listener destroy, commit, new_popup;
};
struct SeraphimLockSurface {
	struct Seraphim *server;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_scene_tree *tree;
	struct wl_listener destroy, commit;
};

struct SeraphimOutput {
	struct wl_list link;
	struct Seraphim *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wl_listener frame, request_state, destroy;
};

struct SeraphimKeyboard {
	struct wl_list link;
	struct Seraphim *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers, key, destroy;
};

struct Seraphim {
	struct wl_display *display;
	struct wl_event_loop *event_loop;
	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_compositor *compositor;
	struct wlr_scene_tree *layers[LAYER_COUNT];
	struct wlr_scene_rect *background;
	struct wlr_output_layout *output_layout;
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wlr_seat *seat;

	struct wlr_layer_shell_v1 *layer_shell;
	struct wlr_session_lock_manager_v1 *session_lock_mgr;
	struct wlr_session_lock_v1 *lock;
	struct wlr_scene_tree *lock_tree;
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel;
	struct wlr_idle_notifier_v1 *idle;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit;
	struct wlr_xdg_activation_v1 *activation;
	struct wlr_cursor_shape_manager_v1 *cursor_shape;
	struct wlr_output_manager_v1 *output_manager;
	struct wlr_screencopy_manager_v1 *screencopy;
	struct wlr_ext_image_copy_capture_manager_v1 *ext_capture;
	struct wlr_gamma_control_manager_v1 *gamma;
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_pointer_constraint_v1 *active_constraint;
	struct wlr_relative_pointer_manager_v1 *relative_pointer;
	struct wlr_text_input_manager_v3 *text_input;
	struct wlr_input_method_manager_v2 *input_method_mgr;
	struct wlr_input_method_v2 *input_method;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
#if SERAPHIM_XWAYLAND
	struct wlr_xwayland *xwayland;
#endif

	struct wlr_surface *keyboard_focus;
	struct wlr_text_input_v3 *focused_text_input;
	bool constraint_activated;

	double last_cursor_x, last_cursor_y;

	struct wl_listener new_lock, lock_new_surface, lock_unlock, lock_destroy;
	struct wl_listener output_apply;
	struct wl_listener activation_request;
	struct wl_listener cursor_request_set_shape;
	struct wl_listener new_constraint, constraint_destroy;
	struct wl_listener new_text_input, text_input_enable, text_input_commit;
	struct wl_listener text_input_disable, text_input_destroy;
	struct wl_listener new_input_method, input_method_commit, input_method_destroy;
	struct wl_listener new_idle_inhibitor, idle_inhibitor_destroy;
	struct wl_listener new_virtual_pointer, new_virtual_keyboard;
#if SERAPHIM_XWAYLAND
	struct wl_listener new_xwayland_surface;
#endif
	struct wl_listener new_layer_surface;

	struct wl_listener new_output, new_xdg_toplevel, new_xdg_popup, new_input;
	struct wl_listener new_xdg_decoration;
	struct wl_listener cursor_motion, cursor_motion_absolute, cursor_button, cursor_axis;
	struct wl_listener request_set_cursor;

	struct wl_list outputs;
	struct wl_list windows;
	struct wl_list keyboards;

	World world;
	Script script;
	int last_focused;
	int buttons_down;
	bool script_started;

	int ipc_fd;
	struct wl_event_source *ipc_source;
	struct wl_event_source *usr1_source;
	char ipc_path[108];

	struct wlr_scene_rect *region_fill;
	struct wlr_scene_rect *region_border[8];
};

static void foreign_sync(struct SeraphimWindow *sw);
static void idle_activity(struct Seraphim *s);
static void text_input_focus_surface(struct Seraphim *s, struct wlr_surface *surface);
static void relative_motion(struct Seraphim *s, uint32_t time_msec, double dx, double dy);
static void constraint_focus_sync(struct Seraphim *s);
static void constraint_apply(struct Seraphim *s);
static bool on_shell_surface(struct Seraphim *s);

static Rect mkbox(double x, double y, double w, double h){
	Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}

static SeraphimWindow *window_by_id(struct Seraphim *s, int id)
{
	SeraphimWindow *sw;

	if (id <= 0)
		return NULL;
	wl_list_for_each(sw, &s->windows, link)
		if (sw->id == id)
			return sw;
	return NULL;
}

static int hit_test(struct Seraphim *s, double sx, double sy)
{
	double wx = camera_to_world_x(&s->world.camera, sx);
	double wy = camera_to_world_y(&s->world.camera, sy);
	return s->world.window_at_point(wx, wy);
}

static void install_signal_handling(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
}

static void crash_handler(int sig)
{
	void **fp = reinterpret_cast<void **>(__builtin_frame_address(0));
	void *base = fp;
	char buf[96];
	int n, i;

	n = snprintf(buf, sizeof(buf), "\nseraphim: killed by signal %d; frames:", sig);
	if (n > 0)
		write(STDERR_FILENO, buf, (size_t)n);
	for (i = 0; i < 32 && fp; i++) {
		ptrdiff_t off = reinterpret_cast<char *>(fp) - reinterpret_cast<char *>(base);

		if (off > 8 * 1024 * 1024 || off < -8 * 1024 * 1024)
			break;
		n = snprintf(buf, sizeof(buf), " %p", fp[1]);
		if (n > 0)
			write(STDERR_FILENO, buf, (size_t)n);
		if (fp[0] <= reinterpret_cast<void *>(fp))
			break;
		fp = reinterpret_cast<void **>(fp[0]);
	}
	write(STDERR_FILENO, "\n", 1);

	signal(sig, SIG_DFL);
	raise(sig);
}

static void install_crash_handler(void)
{
	static const int signals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE };
	size_t i;

	for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
		signal(signals[i], crash_handler);
}

static struct wl_display *g_display;

static bool g_shutting_down;

static void terminate_handler(int sig)
{
	(void)sig;
	if (g_display)
		wl_display_terminate(g_display);
}

static const char *spawn_override(void)
{
	const char *env = getenv("SERAPHIM_SPAWN");

	return (env && *env) ? env : NULL;
}

static void run_spawn_command(void)
{
	const char *override = spawn_override();
	const char *shown = override ? override : spawncmd[0];
	pid_t pid;

	wlr_log(WLR_INFO, "seraphim: spawning %s", shown);

	pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "seraphim: fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		signal(SIGPIPE, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		if (override)
			execl("/bin/sh", "sh", "-c", override, (char *)NULL);
		else
			execvp(spawncmd[0], (char *const *)spawncmd);

		fprintf(stderr, "seraphim: cannot run %s: %s\n", shown, strerror(errno));
		_exit(127);
	}
}

/* A subsurface (foot's scrollback indicator, for one) sits at an offset inside
 * its parent that the client chose in surface-local, unzoomed pixels. wlroots
 * keeps that offset on the subsurface's own scene tree, so zooming has to
 * scale it along with the buffers or the subsurface lands outside the shrunken
 * window. The offset is relative to the parent subsurface, so nested ones
 * scale correctly level by level. */
static void scale_subsurface_offset(struct wlr_scene_buffer *buffer,
		struct wlr_surface *surface, double scale)
{
	struct wlr_subsurface *sub = wlr_subsurface_try_from_wlr_surface(surface);
	struct wlr_scene_tree *parent = buffer->node.parent;

	if (!sub || !parent)
		return;
	if (scale > 0.999 && scale < 1.001)
		wlr_scene_node_set_position(&parent->node, sub->current.x, sub->current.y);
	else
		wlr_scene_node_set_position(&parent->node,
				(int)llround(sub->current.x * scale),
				(int)llround(sub->current.y * scale));
}

static void scale_buffer(struct wlr_scene_buffer *buffer, int sx, int sy, void *data)
{
	double scale = *(double *)data;
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);
	int w, h;

	(void)sx;
	(void)sy;

	if (scene_surface)
		scale_subsurface_offset(buffer, scene_surface->surface, scale);

	if (scale > 0.999 && scale < 1.001) {
		wlr_scene_buffer_set_dest_size(buffer, 0, 0);
		return;
	}

	if (scene_surface && scene_surface->surface->current.width > 0) {
		w = scene_surface->surface->current.width;
		h = scene_surface->surface->current.height;
	} else if (buffer->buffer) {
		w = buffer->buffer->width;
		h = buffer->buffer->height;
	} else
		return;
	wlr_scene_buffer_set_dest_size(buffer,
			(int)(w * scale + 0.5), (int)(h * scale + 0.5));
}

static void place_border(struct wlr_scene_rect *rect, const Rect *box,
		const Rect *origin, const Camera *cam)
{
	int x = (int)llround((box->x - origin->x) * cam->scale);
	int y = (int)llround((box->y - origin->y) * cam->scale);
	int w = (int)llround(box->w * cam->scale);
	int h = (int)llround(box->h * cam->scale);

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	wlr_scene_node_set_position(&rect->node, x, y);
	wlr_scene_rect_set_size(rect, w, h);
}

static void apply_window(struct Seraphim *s, SeraphimWindow *sw)
{
	Window *win = s->world.find(sw->id);
	struct Camera *cam = &s->world.camera;
	const float *inner, *outer;
	BorderRects rings;
	double scale = cam->scale;
	int want_w, want_h, i;
	Rect eff;

	if (!win || !sw->mapped) {
		wlr_scene_node_set_enabled(&sw->tree->node, false);
		sw->applied = false;
		return;
	}

	ApplyKey key = { win->geom.x, win->geom.y, win->geom.w, win->geom.h,
			cam->scale, cam->pan_x, cam->pan_y, sw->shown_w, sw->shown_h,
			win->resize_edges, (int)win->state(&s->world) };
	if (sw->applied && memcmp(&key, &sw->last, sizeof(key)) == 0)
		return;
	sw->last = key;
	sw->applied = true;

	wlr_scene_node_set_enabled(&sw->tree->node, true);

	eff = win->geom;
	if (sw->shown_w > 0 && sw->shown_h > 0) {
		eff.w = sw->shown_w;
		eff.h = sw->shown_h;
		if (win->resize_edges & WEDGE_LEFT)
			eff.x = win->geom.x + win->geom.w - eff.w;
		if (win->resize_edges & WEDGE_TOP)
			eff.y = win->geom.y + win->geom.h - eff.h;
	}

	wlr_scene_node_set_position(&sw->tree->node,
			(int)llround(camera_to_screen_x(cam, eff.x)),
			(int)llround(camera_to_screen_y(cam, eff.y)));

	wlr_scene_node_for_each_buffer(&sw->tree->node, scale_buffer, &scale);

	border_layout(&eff, innerborderpx, outerborderpx, &rings);
	for (i = 0; i < 4; i++) {
		place_border(sw->inner[i], &rings.inner[i], &eff, cam);
		place_border(sw->outer[i], &rings.outer[i], &eff, cam);
	}

	switch (win->state(&s->world)) {
	case WSTATE_FOCUSED:
		inner = focused_inner_border_color;
		outer = focused_outer_border_color;
		break;
	case WSTATE_URGENT:
		inner = urgent_inner_border_color;
		outer = urgent_outer_border_color;
		break;
	default:
		inner = unfocused_inner_border_color;
		outer = unfocused_outer_border_color;
		break;
	}
	for (i = 0; i < 4; i++) {
		wlr_scene_rect_set_color(sw->inner[i], inner);
		wlr_scene_rect_set_color(sw->outer[i], outer);
	}

	want_w = (int)llround(win->geom.w);
	want_h = (int)llround(win->geom.h);
	if (want_w < 1)
		want_w = 1;
	if (want_h < 1)
		want_h = 1;
#if SERAPHIM_XWAYLAND
	if (sw->xsurface) {
		if (want_w != sw->want_w || want_h != sw->want_h) {
			sw->want_w = want_w;
			sw->want_h = want_h;
			wlr_xwayland_surface_configure(sw->xsurface, sw->xsurface->x,
					sw->xsurface->y, (uint16_t)want_w, (uint16_t)want_h);
		}
		return;
	}
#endif
	if (sw->toplevel && (want_w != sw->want_w || want_h != sw->want_h)) {
		sw->want_w = want_w;
		sw->want_h = want_h;
		wlr_xdg_toplevel_set_size(sw->toplevel, want_w, want_h);

		sw->want_serial = wlr_xdg_surface_schedule_configure(sw->toplevel->base);
	}
}

static void place_screen_rect(struct wlr_scene_rect *rect, const Rect *box)
{
	int w = (int)llround(box->w);
	int h = (int)llround(box->h);

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	wlr_scene_node_set_position(&rect->node, (int)llround(box->x), (int)llround(box->y));
	wlr_scene_rect_set_size(rect, w, h);
	wlr_scene_node_set_enabled(&rect->node, true);
}

static void region_draw(struct Seraphim *s, const Rect *box)
{
#if SPAWNREGION_STYLE == 1
	place_screen_rect(s->region_fill, box);
#elif SPAWNREGION_STYLE == 2

	Rect content = mkbox(box->x + spawnregion_innerborderpx,
			box->y + spawnregion_innerborderpx,
			box->w - 2 * spawnregion_innerborderpx,
			box->h - 2 * spawnregion_innerborderpx);
	BorderRects rings;
	int i;

	border_layout(&content, spawnregion_innerborderpx, spawnregion_outerborderpx, &rings);
	for (i = 0; i < 4; i++) {
		place_screen_rect(s->region_border[i], &rings.inner[i]);
		place_screen_rect(s->region_border[4 + i], &rings.outer[i]);
	}
#else
	(void)s;
	(void)box;
#endif
}

static void apply_region(struct Seraphim *s)
{
#if SPAWNREGION_STYLE != 0
	Rect box;
	int i;

	if (!s->world.region_active) {
		if (s->region_fill)
			wlr_scene_node_set_enabled(&s->region_fill->node, false);
		for (i = 0; i < 8; i++)
			if (s->region_border[i])
				wlr_scene_node_set_enabled(&s->region_border[i]->node, false);
		return;
	}

	box = mkbox(fmin(s->world.rx1, s->world.rx2), fmin(s->world.ry1, s->world.ry2),
			fabs(s->world.rx2 - s->world.rx1), fabs(s->world.ry2 - s->world.ry1));
	region_draw(s, &box);
#else
	(void)s;
#endif
}

static struct wlr_surface *window_surface(SeraphimWindow *sw)
{
	if (!sw)
		return NULL;
	if (sw->toplevel)
		return sw->toplevel->base->surface;
#if SERAPHIM_XWAYLAND
	if (sw->xsurface)
		return sw->xsurface->surface;
#endif
	return NULL;
}

static void seat_focus_window(struct Seraphim *s, SeraphimWindow *sw)
{
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);
	struct wlr_surface *surface = NULL;

	if (sw) {
		if (sw->toplevel)
			surface = sw->toplevel->base->surface;
#if SERAPHIM_XWAYLAND
		else if (sw->xsurface)
			surface = sw->xsurface->surface;
#endif
	}
	if (!surface) {
		wlr_seat_keyboard_notify_clear_focus(s->seat);
		return;
	}
	if (kb)
		wlr_seat_keyboard_notify_enter(s->seat, surface, kb->keycodes,
				kb->num_keycodes, &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(s->seat, surface, NULL, 0, NULL);
}

static void run_shell_command(const char *cmd)
{
	pid_t pid;

	wlr_log(WLR_INFO, "seraphim: running %s", cmd);
	pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "seraphim: fork failed: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		signal(SIGPIPE, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		fprintf(stderr, "seraphim: cannot run %s: %s\n", cmd, strerror(errno));
		_exit(127);
	}
}

static void apply_world(struct Seraphim *s)
{
	SeraphimWindow *sw, *focused_window;

	if (g_shutting_down)
		return;
	size_t i;
	int id, focused;

	if (s->world.reload_request) {
		s->world.reload_request = false;
		if (!s->script.reload())
			wlr_log(WLR_ERROR, "seraphim: %s", s->script.error.c_str());
	}

	struct wlr_scene_node *prev = NULL;

	for (i = 0; i < s->world.windows.size(); i++) {
		sw = window_by_id(s, s->world.windows[i].id);
		if (!sw)
			continue;
		apply_window(s, sw);
		if (prev && sw->tree->node.link.prev != &prev->link)
			wlr_scene_node_raise_to_top(&sw->tree->node);
		prev = &sw->tree->node;
	}

	while ((id = world_take_close(&s->world)) != 0) {
		sw = window_by_id(s, id);
		if (sw && sw->toplevel)
			wlr_xdg_toplevel_send_close(sw->toplevel);
	}

	if (world_take_spawn_request(&s->world))
		run_spawn_command();

	{
		std::string cmd;

		while (world_take_queued_spawn(&s->world, &cmd))
			run_shell_command(cmd.c_str());
	}

	if (s->world.quit_request) {
		s->world.quit_request = false;
		wl_display_terminate(s->display);
	}

	apply_region(s);

	focused = s->world.focused_id;
	if (focused != s->last_focused) {
		focused_window = window_by_id(s, focused);
		wl_list_for_each(sw, &s->windows, link) {
			bool active = sw == focused_window;

			if (sw->toplevel)
				wlr_xdg_toplevel_set_activated(sw->toplevel, active);
#if SERAPHIM_XWAYLAND
			else if (sw->xsurface)
				wlr_xwayland_surface_activate(sw->xsurface, active);
#endif
			if (sw->ftl)
				wlr_foreign_toplevel_handle_v1_set_activated(sw->ftl, active);
		}
		if (!s->keyboard_focus) {
			seat_focus_window(s, focused_window);
			text_input_focus_surface(s, window_surface(focused_window));
		}
		s->last_focused = focused;
	}
}

static void update_view(struct Seraphim *s)
{
	struct wlr_box box;

	wlr_output_layout_get_box(s->output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0)
		return;

	s->world.view_w = box.width;
	s->world.view_h = box.height;

	if (s->background) {
		wlr_scene_node_set_position(&s->background->node, box.x, box.y);
		wlr_scene_rect_set_size(s->background, box.width, box.height);
	}

	if (s->script.loaded && !s->script_started) {
		s->script_started = true;
		s->script.call_init();
	}
}

static void output_frame(struct wl_listener *listener, void *data)
{
	struct SeraphimOutput *so = wl_container_of(listener, so, frame);
	struct Seraphim *s = so->server;
	struct timespec now;
	SeraphimWindow *sw;
	double scale = s->world.camera.scale;

	(void)data;

	if (scale < 0.999 || scale > 1.001) {
		wl_list_for_each(sw, &s->windows, link) {
			if (sw->id && sw->mapped)
				wlr_scene_node_for_each_buffer(&sw->tree->node, scale_buffer,
						&scale);
		}
	}

	wlr_scene_output_commit(so->scene_output, NULL);
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(so->scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data)
{
	struct SeraphimOutput *so = wl_container_of(listener, so, request_state);
	struct wlr_output_event_request_state *event =
			static_cast<struct wlr_output_event_request_state *>(data);

	wlr_output_commit_state(so->wlr_output, event->state);
	update_view(so->server);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimOutput *so = wl_container_of(listener, so, destroy);
	struct Seraphim *s = so->server;

	(void)data;
	wl_list_remove(&so->frame.link);
	wl_list_remove(&so->request_state.link);
	wl_list_remove(&so->destroy.link);
	wl_list_remove(&so->link);
	free(so);
	update_view(s);
}

static void server_new_output(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_output);
	struct wlr_output *wlr_output = static_cast<struct wlr_output *>(data);
	struct SeraphimOutput *so;
	struct wlr_output_state state;

	wlr_output_init_render(wlr_output, s->allocator, s->renderer);

	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	if (wlr_output->phys_width == 0) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		if (mode)
			wlr_output_state_set_mode(&state, mode);
	}
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	so = static_cast<struct SeraphimOutput *>(calloc(1, sizeof(*so)));
	if (!so) {
		wlr_log(WLR_ERROR, "seraphim: cannot allocate an output");
		return;
	}
	so->server = s;
	so->wlr_output = wlr_output;
	so->scene_output = wlr_scene_output_create(s->scene, wlr_output);
	wlr_output_layout_add_auto(s->output_layout, wlr_output);

	so->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &so->frame);
	so->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &so->request_state);
	so->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &so->destroy);

	wl_list_insert(&s->outputs, &so->link);
	update_view(s);

	wlr_log(WLR_INFO, "seraphim: output %s %dx%d", wlr_output->name,
			wlr_output->width, wlr_output->height);
}

static void decoration_apply(struct SeraphimWindow *sw);

static void sync_window_meta(struct SeraphimWindow *sw, bool notify)
{
	struct Seraphim *s = sw->server;
	const char *title = NULL, *app_id = NULL;
	std::string t, a;
	Window *win;

	if (!sw->id)
		return;
	win = s->world.find(sw->id);
	if (!win)
		return;
	if (sw->toplevel) {
		title = sw->toplevel->title;
		app_id = sw->toplevel->app_id;
	}
	t = title ? title : "";
	a = app_id ? app_id : "";
	if (t == win->title && a == win->app_id)
		return;
	win->title = t;
	win->app_id = a;
	if (notify) {
		s->script.call_window_changed(sw->id);
		apply_world(s);
	}
}

static void xdg_toplevel_title(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, xdg_title);

	(void)data;
	sync_window_meta(sw, true);
}

static void xdg_toplevel_app_id(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, xdg_app_id);

	(void)data;
	sync_window_meta(sw, true);
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, map);
	struct Seraphim *s = sw->server;
	Rect geom;

	(void)data;

	if (!world_take_pending_geometry(&s->world, &geom))
		geom = world_default_geometry(&s->world);

	sw->id = s->world.add(geom, sw);
	sw->mapped = true;
	wlr_log(WLR_INFO, "seraphim: window %d up at %g,%g %gx%g", sw->id,
			geom.x, geom.y, geom.w, geom.h);
	sw->want_w = (int)llround(geom.w);
	sw->want_h = (int)llround(geom.h);
	if (sw->toplevel) {
		wlr_xdg_toplevel_set_size(sw->toplevel, sw->want_w, sw->want_h);
		sw->want_serial = wlr_xdg_surface_schedule_configure(sw->toplevel->base);
	}

	s->world.focus(sw->id);
	sync_window_meta(sw, false);
	s->script.call_window_open(sw->id);
	apply_world(s);
	foreign_sync(sw);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, unmap);
	struct Seraphim *s = sw->server;
	int id = sw->id;

	(void)data;
	sw->mapped = false;
	sw->id = 0;
	if (id) {
		s->world.remove(id);
		s->script.call_window_close(id);
	}
	apply_world(s);
	foreign_sync(sw);
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, commit);
	struct Seraphim *s = sw->server;
	struct wlr_box geo;
	Window *win;

	(void)data;
	if (!sw->toplevel)
		return;

	if (sw->toplevel->base->initial_commit) {
		Rect geom;

		if (s->world.pending_spawn.w > 0.0 && s->world.pending_spawn.h > 0.0)
			geom = s->world.pending_spawn;
		else
			geom = world_default_geometry(&s->world);

		decoration_apply(sw);
		wlr_xdg_toplevel_set_size(sw->toplevel, (int)llround(geom.w),
				(int)llround(geom.h));
		wlr_xdg_surface_schedule_configure(sw->toplevel->base);
		return;
	}

	win = s->world.find(sw->id);
	if (!win)
		return;

	geo = sw->toplevel->base->geometry;
	if (geo.width > 0 && geo.height > 0 &&
			(geo.width != sw->shown_w || geo.height != sw->shown_h)) {
		sw->shown_w = geo.width;
		sw->shown_h = geo.height;
		apply_world(s);
	}

	if (sw->sized && sw->shown_w > 0 &&
			sw->toplevel->base->current.configure_serial == sw->want_serial &&
			(sw->shown_w != (int)llround(win->geom.w) ||
			 sw->shown_h != (int)llround(win->geom.h))) {
		if (win->resize_edges & WEDGE_LEFT)
			win->geom.x += win->geom.w - sw->shown_w;
		if (win->resize_edges & WEDGE_TOP)
			win->geom.y += win->geom.h - sw->shown_h;
		win->geom.w = sw->shown_w;
		win->geom.h = sw->shown_h;
		sw->want_w = sw->shown_w;
		sw->want_h = sw->shown_h;
		apply_world(s);
	}

	if (!sw->sized) {
		geo = sw->toplevel->base->geometry;
		sw->sized = true;
		if (geo.width > 0 && geo.height > 0 &&
				(geo.width != (int)llround(win->geom.w) ||
				 geo.height != (int)llround(win->geom.h))) {
			win->geom.w = geo.width;
			win->geom.h = geo.height;
			apply_world(s);
		}
	}
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, destroy);
	struct Seraphim *s = sw->server;
	int id = sw->id;

	(void)data;
	if (id) {
		s->world.remove(id);
		s->script.call_window_close(id);
		wlr_log(WLR_INFO, "seraphim: window %d gone", id);
		apply_world(s);
	}

	if (sw->ftl) {
		struct wlr_foreign_toplevel_handle_v1 *handle = sw->ftl;

		sw->ftl = NULL;
		wlr_foreign_toplevel_handle_v1_destroy(handle);
	}
	if (sw->deco) {
		wl_list_remove(&sw->deco_request_mode.link);
		wl_list_remove(&sw->deco_destroy.link);
	}
	wl_list_remove(&sw->xdg_title.link);
	wl_list_remove(&sw->xdg_app_id.link);
	wl_list_remove(&sw->map.link);
	wl_list_remove(&sw->unmap.link);
	wl_list_remove(&sw->commit.link);
	wl_list_remove(&sw->destroy.link);
	wl_list_remove(&sw->link);

	free(sw);
}

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_xdg_toplevel);
	struct wlr_xdg_toplevel *toplevel = static_cast<struct wlr_xdg_toplevel *>(data);
	struct wlr_surface *surface = toplevel->base->surface;
	SeraphimWindow *sw = static_cast<SeraphimWindow *>(calloc(1, sizeof(*sw)));
	int i;

	if (!sw) {
		wlr_log(WLR_ERROR, "seraphim: cannot allocate a window");
		return;
	}
	sw->server = s;
	sw->toplevel = toplevel;
	sw->tree = wlr_scene_xdg_surface_create(s->layers[LAYER_WINDOWS], toplevel->base);

	toplevel->base->data = sw->tree;
	for (i = 0; i < 4; i++) {
		sw->inner[i] = wlr_scene_rect_create(sw->tree, 1, 1,
				unfocused_inner_border_color);
		sw->outer[i] = wlr_scene_rect_create(sw->tree, 1, 1,
				unfocused_outer_border_color);
	}
	wlr_scene_node_set_enabled(&sw->tree->node, false);

	sw->map.notify = xdg_toplevel_map;
	wl_signal_add(&surface->events.map, &sw->map);
	sw->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&surface->events.unmap, &sw->unmap);
	sw->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&surface->events.commit, &sw->commit);
	sw->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&toplevel->events.destroy, &sw->destroy);
	sw->xdg_title.notify = xdg_toplevel_title;
	wl_signal_add(&toplevel->events.set_title, &sw->xdg_title);
	sw->xdg_app_id.notify = xdg_toplevel_app_id;
	wl_signal_add(&toplevel->events.set_app_id, &sw->xdg_app_id);

	wl_list_insert(&s->windows, &sw->link);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_xdg_popup);
	struct wlr_xdg_popup *popup = static_cast<struct wlr_xdg_popup *>(data);
	struct wlr_xdg_surface *parent = popup->parent
			? wlr_xdg_surface_try_from_wlr_surface(popup->parent) : NULL;
	struct wlr_scene_tree *parent_tree = (parent && parent->data)
			? static_cast<struct wlr_scene_tree *>(parent->data)
			: s->layers[LAYER_WINDOWS];

	popup->base->data = wlr_scene_xdg_surface_create(parent_tree, popup->base);
}

static void decoration_apply(struct SeraphimWindow *sw)
{
	if (!sw->deco || !sw->toplevel)
		return;
	wlr_xdg_toplevel_decoration_v1_set_mode(sw->deco,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void decoration_request_mode(struct wl_listener *listener, void *data)
{
	struct SeraphimWindow *sw = wl_container_of(listener, sw, deco_request_mode);

	(void)data;

	if (sw->toplevel && sw->toplevel->base->initialized)
		decoration_apply(sw);
}

static void decoration_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimWindow *sw = wl_container_of(listener, sw, deco_destroy);

	(void)data;
	wl_list_remove(&sw->deco_request_mode.link);
	wl_list_remove(&sw->deco_destroy.link);
	sw->deco = NULL;
}

static void server_new_xdg_decoration(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_xdg_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco =
			static_cast<struct wlr_xdg_toplevel_decoration_v1 *>(data);
	SeraphimWindow *sw, *found = NULL;

	wl_list_for_each(sw, &s->windows, link) {
		if (sw->toplevel == wlr_deco->toplevel) {
			found = sw;
			break;
		}
	}
	if (!found) {
		wlr_log(WLR_ERROR, "seraphim: a decoration arrived for an unknown toplevel");
		return;
	}

	found->deco = wlr_deco;
	found->deco_request_mode.notify = decoration_request_mode;
	wl_signal_add(&wlr_deco->events.request_mode, &found->deco_request_mode);
	found->deco_destroy.notify = decoration_destroy;
	wl_signal_add(&wlr_deco->events.destroy, &found->deco_destroy);
}

static unsigned mods_from_xkb(struct wlr_keyboard *kb)
{
	struct xkb_state *state = kb->xkb_state;
	unsigned mods = 0;

	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
		mods |= SERAPHIM_MOD_SHIFT;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)
		mods |= SERAPHIM_MOD_CTRL;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
		mods |= SERAPHIM_MOD_ALT;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0)
		mods |= SERAPHIM_MOD_LOGO;
	return mods;
}

static unsigned current_mods(struct Seraphim *s)
{
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(s->seat);

	return kb ? mods_from_xkb(kb) : 0;
}

static struct wlr_surface *surface_under(struct Seraphim *s, double sx, double sy,
		double *nx, double *ny)
{
	struct wlr_scene_node *node = wlr_scene_node_at(&s->scene->tree.node, sx, sy, nx, ny);

	if (!node || node->type != WLR_SCENE_NODE_BUFFER)
		return NULL;
	struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);

	return scene_surface ? scene_surface->surface : NULL;
}

static void process_cursor_motion(struct Seraphim *s, uint32_t time)
{
	double sx = s->cursor->x, sy = s->cursor->y;
	double nx = 0, ny = 0;
	struct wlr_surface *surface;
	int hit;

	hit = hit_test(s, sx, sy);
	if (s->buttons_down > 0) {
		s->script.call_motion((float)sx, (float)sy, hit, current_mods(s));
		apply_world(s);
	} else if (sloppy_focus && hit > 0 && hit != s->world.focused_id &&
			!s->lock && !on_shell_surface(s)) {
		s->world.focused_id = hit;
		apply_world(s);
	}

	surface = surface_under(s, sx, sy, &nx, &ny);
	if (surface) {
		wlr_seat_pointer_notify_enter(s->seat, surface, nx, ny);
		wlr_seat_pointer_notify_motion(s->seat, time, nx, ny);
	} else {
		wlr_seat_pointer_clear_focus(s->seat);
	}

	constraint_focus_sync(s);
	constraint_apply(s);
}

static void cursor_motion(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, cursor_motion);
	struct wlr_pointer_motion_event *event =
			static_cast<struct wlr_pointer_motion_event *>(data);

	wlr_cursor_move(s->cursor, &event->pointer->base, event->delta_x, event->delta_y);
	relative_motion(s, event->time_msec, event->delta_x, event->delta_y);
	idle_activity(s);
	process_cursor_motion(s, event->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event =
			static_cast<struct wlr_pointer_motion_absolute_event *>(data);

	wlr_cursor_warp_absolute(s->cursor, &event->pointer->base, event->x, event->y);
	idle_activity(s);
	process_cursor_motion(s, event->time_msec);
}

static void cursor_button(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, cursor_button);
	struct wlr_pointer_button_event *event =
			static_cast<struct wlr_pointer_button_event *>(data);
	bool pressed = event->state == WL_POINTER_BUTTON_STATE_PRESSED;
	bool consumed;

	if (pressed)
		s->buttons_down++;
	else if (s->buttons_down > 0)
		s->buttons_down--;

	idle_activity(s);

	if (s->lock || on_shell_surface(s)) {
		wlr_seat_pointer_notify_button(s->seat, event->time_msec, event->button,
				event->state);
		return;
	}

	consumed = s->script.call_button(event->button, pressed,
			(float)s->cursor->x, (float)s->cursor->y,
			hit_test(s, s->cursor->x, s->cursor->y), current_mods(s));
	apply_world(s);

	{
		double wx, wy;

		if (world_take_warp(&s->world, &wx, &wy))
			wlr_cursor_warp(s->cursor, NULL, wx, wy);
	}

	if (consumed)
		return;

	wlr_seat_pointer_notify_button(s->seat, event->time_msec, event->button,
			event->state);
}

static void cursor_axis(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, cursor_axis);
	struct wlr_pointer_axis_event *event =
			static_cast<struct wlr_pointer_axis_event *>(data);
	double nx = 0, ny = 0;
	bool over_client;

	over_client = surface_under(s, s->cursor->x, s->cursor->y, &nx, &ny) != NULL;

	if (over_client && (current_mods(s) & MODKEY) == 0) {
		wlr_seat_pointer_notify_axis(s->seat, event->time_msec, event->orientation,
				event->delta, event->delta_discrete, event->source,
				event->relative_direction);
		return;
	}
	s->script.call_axis((float)event->delta, (float)s->cursor->x, (float)s->cursor->y,
			current_mods(s));
	apply_world(s);
}

static bool host_key(struct Seraphim *s, xkb_keysym_t sym, unsigned mods)
{
	unsigned ctrl_alt = SERAPHIM_MOD_CTRL | SERAPHIM_MOD_ALT;

	if (sym == SERAPHIM_KEY_ESCAPE && (mods & MODKEY)) {
		wlr_log(WLR_INFO, "seraphim: quitting");
		wl_display_terminate(s->display);
		return true;
	}

	if ((sym == SERAPHIM_KEY_Q || sym == SERAPHIM_KEY_Q_UPPER) &&
			(mods & MODKEY) && (mods & SERAPHIM_MOD_SHIFT)) {
		wlr_log(WLR_INFO, "seraphim: quitting");
		wl_display_terminate(s->display);
		return true;
	}
	if ((mods & ctrl_alt) == ctrl_alt && sym >= SERAPHIM_KEY_F1 &&
			sym < SERAPHIM_KEY_F1 + 12) {
		unsigned vt = 1 + (unsigned)(sym - SERAPHIM_KEY_F1);

		if (!s->session) {
			wlr_log(WLR_INFO, "seraphim: no session to switch VT on -- "
					"this build is running nested");
			return true;
		}
		if (!wlr_session_change_vt(s->session, vt))
			wlr_log(WLR_ERROR, "seraphim: cannot switch to VT %u", vt);
		return true;
	}
	return false;
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	struct SeraphimKeyboard *skb = wl_container_of(listener, skb, modifiers);

	(void)data;
	wlr_seat_set_keyboard(skb->server->seat, skb->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(skb->server->seat,
			&skb->wlr_keyboard->modifiers);
}

static void keyboard_handle_key(struct wl_listener *listener, void *data)
{
	struct SeraphimKeyboard *skb = wl_container_of(listener, skb, key);
	struct Seraphim *s = skb->server;
	struct wlr_keyboard_key_event *event =
			static_cast<struct wlr_keyboard_key_event *>(data);
	const xkb_keysym_t *syms;
	int nsyms, i;
	bool handled = false;

	nsyms = xkb_state_key_get_syms(skb->wlr_keyboard->xkb_state,
			event->keycode + 8, &syms);

	for (i = 0; i < nsyms; i++) {
		unsigned mods = mods_from_xkb(skb->wlr_keyboard);
		bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;

		idle_activity(s);

		if (pressed && host_key(s, syms[i], mods)) {
			handled = true;
			continue;
		}

		if (!s->keyboard_focus && s->script.call_key(syms[i], mods, pressed))
			handled = true;
	}
	apply_world(s);

	if (handled)
		return;
	wlr_seat_set_keyboard(s->seat, skb->wlr_keyboard);
	wlr_seat_keyboard_notify_key(s->seat, event->time_msec, event->keycode,
			event->state);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimKeyboard *skb = wl_container_of(listener, skb, destroy);

	(void)data;
	wl_list_remove(&skb->modifiers.link);
	wl_list_remove(&skb->key.link);
	wl_list_remove(&skb->destroy.link);
	wl_list_remove(&skb->link);
	free(skb);
}

static void add_keyboard(struct Seraphim *s, struct wlr_keyboard *wlr_keyboard)
{
	struct SeraphimKeyboard *skb;

	skb = static_cast<struct SeraphimKeyboard *>(calloc(1, sizeof(*skb)));
	if (!skb) {
		wlr_log(WLR_ERROR, "seraphim: cannot allocate a keyboard");
		return;
	}
	wlr_keyboard_set_repeat_info(wlr_keyboard, key_repeat_rate, key_repeat_delay);

	skb->server = s;
	skb->wlr_keyboard = wlr_keyboard;
	skb->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &skb->modifiers);
	skb->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &skb->key);
	skb->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&wlr_keyboard->base.events.destroy, &skb->destroy);

	wl_list_insert(&s->keyboards, &skb->link);
	wlr_seat_set_keyboard(s->seat, wlr_keyboard);
}

static void server_new_keyboard(struct Seraphim *s, struct wlr_input_device *device)
{
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap;

	if (!context) {
		wlr_log(WLR_ERROR, "seraphim: cannot create an xkb context");
		return;
	}

	keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		wlr_log(WLR_ERROR, "seraphim: cannot compile an xkb keymap");
		xkb_context_unref(context);
		return;
	}
	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	add_keyboard(s, wlr_keyboard);
}

static void server_new_input(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_input);
	struct wlr_input_device *device = static_cast<struct wlr_input_device *>(data);
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(s, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		wlr_cursor_attach_input_device(s->cursor, device);
		break;
	default:
		break;
	}

	if (!wl_list_empty(&s->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(s->seat, caps);
}

static void seat_request_set_cursor(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event =
			static_cast<struct wlr_seat_pointer_request_set_cursor_event *>(data);
	struct wlr_seat_client *focused = s->seat->pointer_state.focused_client;

	if (focused && focused == event->seat_client)
		wlr_cursor_set_surface(s->cursor, event->surface, event->hotspot_x,
				event->hotspot_y);
}

static struct SeraphimOutput *first_output(struct Seraphim *s)
{
	struct SeraphimOutput *so;

	wl_list_for_each(so, &s->outputs, link)
		return so;
	return NULL;
}

static bool on_shell_surface(struct Seraphim *s)
{
	double nx = 0, ny = 0;
	struct wlr_surface *surface =
			surface_under(s, s->cursor->x, s->cursor->y, &nx, &ny);

	if (!surface)
		return false;
	if (wlr_layer_surface_v1_try_from_wlr_surface(surface))
		return true;
	if (wlr_session_lock_surface_v1_try_from_wlr_surface(surface))
		return true;
	return false;
}

static void idle_activity(struct Seraphim *s)
{
	if (!s->idle)
		return;
	if (s->idle_inhibit)
		wlr_idle_notifier_v1_set_inhibited(s->idle,
				!wl_list_empty(&s->idle_inhibit->inhibitors));
	wlr_idle_notifier_v1_notify_activity(s->idle, s->seat);
}

struct SeraphimInhibitor {
	struct Seraphim *server;
	struct wl_listener destroy;
};

static void idle_inhibitor_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimInhibitor *link = wl_container_of(listener, link, destroy);
	struct Seraphim *s = link->server;

	(void)data;
	wl_list_remove(&link->destroy.link);
	free(link);
	if (s->idle && s->idle_inhibit)
		wlr_idle_notifier_v1_set_inhibited(s->idle,
				!wl_list_empty(&s->idle_inhibit->inhibitors));
}

static void idle_inhibitor_new(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_idle_inhibitor);
	struct wlr_idle_inhibitor_v1 *inhibitor =
			static_cast<struct wlr_idle_inhibitor_v1 *>(data);
	struct SeraphimInhibitor *link =
			static_cast<struct SeraphimInhibitor *>(calloc(1, sizeof(*link)));

	if (!link)
		return;
	link->server = s;
	link->destroy.notify = idle_inhibitor_destroy;
	wl_signal_add(&inhibitor->events.destroy, &link->destroy);
	if (s->idle)
		wlr_idle_notifier_v1_set_inhibited(s->idle, true);
}

struct SeraphimForeign {
	struct SeraphimWindow *window;
	struct wl_listener activate, close, destroy;
};

static void foreign_activate(struct wl_listener *listener, void *data)
{
	struct SeraphimForeign *fl = wl_container_of(listener, fl, activate);
	struct Seraphim *s = fl->window->server;

	(void)data;
	if (fl->window->id <= 0)
		return;
	s->world.focus(fl->window->id);
	apply_world(s);
}

static void foreign_close(struct wl_listener *listener, void *data)
{
	struct SeraphimForeign *fl = wl_container_of(listener, fl, close);
	struct Seraphim *s = fl->window->server;

	(void)data;
	if (fl->window->id > 0) {
		world_request_close(&s->world, fl->window->id);
		apply_world(s);
	}
}

static void foreign_link_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimForeign *fl = wl_container_of(listener, fl, destroy);

	(void)data;
	wl_list_remove(&fl->activate.link);
	wl_list_remove(&fl->close.link);
	wl_list_remove(&fl->destroy.link);
	free(fl);
}

static void foreign_sync(struct SeraphimWindow *sw)
{
	struct Seraphim *s = sw->server;
	const char *title = NULL;
	const char *app_id = NULL;

	if (!s->foreign_toplevel)
		return;

	if (!sw->mapped || sw->override_redirect) {
		if (sw->ftl) {
			struct wlr_foreign_toplevel_handle_v1 *handle = sw->ftl;

			sw->ftl = NULL;
			wlr_foreign_toplevel_handle_v1_destroy(handle);
		}
		return;
	}

	if (!sw->ftl) {
		struct SeraphimForeign *fl;
		struct SeraphimOutput *so;

		sw->ftl = wlr_foreign_toplevel_handle_v1_create(s->foreign_toplevel);
		if (!sw->ftl)
			return;
		fl = static_cast<struct SeraphimForeign *>(calloc(1, sizeof(*fl)));
		if (fl) {
			fl->window = sw;
			fl->activate.notify = foreign_activate;
			wl_signal_add(&sw->ftl->events.request_activate, &fl->activate);
			fl->close.notify = foreign_close;
			wl_signal_add(&sw->ftl->events.request_close, &fl->close);
			fl->destroy.notify = foreign_link_destroy;
			wl_signal_add(&sw->ftl->events.destroy, &fl->destroy);
			sw->ftl->data = fl;
		}
		so = first_output(s);
		if (so)
			wlr_foreign_toplevel_handle_v1_output_enter(sw->ftl, so->wlr_output);
	}

	if (sw->toplevel) {
		title = sw->toplevel->title;
		app_id = sw->toplevel->app_id;
	}
#if SERAPHIM_XWAYLAND
	else if (sw->xsurface) {
		title = sw->xsurface->title;
		app_id = sw->xsurface->class_;
	}
#endif
	if (title)
		wlr_foreign_toplevel_handle_v1_set_title(sw->ftl, title);
	if (app_id)
		wlr_foreign_toplevel_handle_v1_set_app_id(sw->ftl, app_id);
	wlr_foreign_toplevel_handle_v1_set_activated(sw->ftl,
			sw->id > 0 && s->world.focused_id == sw->id);
}

static struct wlr_scene_tree *layer_tree_for(struct Seraphim *s, uint32_t layer)
{
	switch (layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return s->layers[LAYER_LS_BACKGROUND];
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return s->layers[LAYER_LS_BOTTOM];
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return s->layers[LAYER_LS_TOP];
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
	default:
		return s->layers[LAYER_LS_OVERLAY];
	}
}

static void layer_arrange(struct SeraphimLayer *sl)
{
	struct Seraphim *s = sl->server;
	struct wlr_layer_surface_v1 *ls = sl->layer_surface;
	struct wlr_box full = {0, 0, 0, 0};
	struct wlr_box usable;

	if (!ls->output) {
		struct SeraphimOutput *so = first_output(s);

		if (!so)
			return;
		ls->output = so->wlr_output;
	}
	wlr_output_layout_get_box(s->output_layout, ls->output, &full);
	usable = full;

	wlr_scene_layer_surface_v1_configure(sl->scene_layer, &full, &usable);

	if (ls->current.keyboard_interactive !=
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
		s->keyboard_focus = ls->surface;
		wlr_seat_keyboard_notify_enter(s->seat, ls->surface, NULL, 0, NULL);
	}
}

static void layer_commit(struct wl_listener *listener, void *data)
{
	struct SeraphimLayer *sl = wl_container_of(listener, sl, commit);

	(void)data;
	layer_arrange(sl);
}

static void layer_new_popup(struct wl_listener *listener, void *data)
{
	struct SeraphimLayer *sl = wl_container_of(listener, sl, new_popup);
	struct Seraphim *s = sl->server;
	struct wlr_xdg_popup *popup = static_cast<struct wlr_xdg_popup *>(data);
	struct wlr_box box = {0, 0, 0, 0};

	popup->base->data = wlr_scene_xdg_surface_create(sl->scene_layer->tree, popup->base);
	if (sl->layer_surface->output)
		wlr_output_layout_get_box(s->output_layout, sl->layer_surface->output, &box);
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
}

static void layer_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimLayer *sl = wl_container_of(listener, sl, destroy);
	struct Seraphim *s = sl->server;

	(void)data;
	wl_list_remove(&sl->destroy.link);
	wl_list_remove(&sl->commit.link);
	wl_list_remove(&sl->new_popup.link);

	if (s->keyboard_focus == sl->layer_surface->surface) {
		s->keyboard_focus = NULL;
		s->last_focused = -1;
	}
	free(sl);
}

static void server_new_layer_surface(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_layer_surface);
	struct wlr_layer_surface_v1 *ls = static_cast<struct wlr_layer_surface_v1 *>(data);
	struct SeraphimLayer *sl =
			static_cast<struct SeraphimLayer *>(calloc(1, sizeof(*sl)));

	if (!sl) {
		wlr_log(WLR_ERROR, "seraphim: cannot allocate a layer surface");
		return;
	}
	if (!ls->output) {
		struct SeraphimOutput *so = first_output(s);

		if (so)
			ls->output = so->wlr_output;
	}

	sl->server = s;
	sl->layer_surface = ls;
	sl->scene_layer = wlr_scene_layer_surface_v1_create(
			layer_tree_for(s, ls->pending.layer), ls);
	if (!sl->scene_layer) {
		free(sl);
		return;
	}

	sl->destroy.notify = layer_destroy;
	wl_signal_add(&ls->events.destroy, &sl->destroy);
	sl->commit.notify = layer_commit;
	wl_signal_add(&ls->surface->events.commit, &sl->commit);
	sl->new_popup.notify = layer_new_popup;
	wl_signal_add(&ls->events.new_popup, &sl->new_popup);

	layer_arrange(sl);

	wlr_log(WLR_INFO, "seraphim: layer surface %s",
			ls->namespace_ ? ls->namespace_ : "?");
}

static void lock_surface_commit(struct wl_listener *listener, void *data);

static uint32_t lock_surface_configure(struct SeraphimLockSurface *lsl)
{
	int width = 0, height = 0;

	if (lsl->lock_surface->output) {
		width = lsl->lock_surface->output->width;
		height = lsl->lock_surface->output->height;
	}
	return wlr_session_lock_surface_v1_configure(lsl->lock_surface,
			(uint32_t)width, (uint32_t)height);
}

static void lock_surface_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimLockSurface *lsl = wl_container_of(listener, lsl, destroy);

	(void)data;
	wl_list_remove(&lsl->destroy.link);
	wl_list_remove(&lsl->commit.link);
	free(lsl);
}

static void lock_surface_commit(struct wl_listener *listener, void *data)
{
	struct SeraphimLockSurface *lsl = wl_container_of(listener, lsl, commit);

	(void)data;
	lock_surface_configure(lsl);
}

static void lock_new_surface(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, lock_new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface =
			static_cast<struct wlr_session_lock_surface_v1 *>(data);
	struct SeraphimLockSurface *lsl =
			static_cast<struct SeraphimLockSurface *>(calloc(1, sizeof(*lsl)));

	if (!lsl)
		return;
	lsl->server = s;
	lsl->lock_surface = lock_surface;
	lsl->tree = wlr_scene_tree_create(s->lock_tree);
	if (lsl->tree)
		wlr_scene_surface_create(lsl->tree, lock_surface->surface);

	lsl->destroy.notify = lock_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &lsl->destroy);
	lsl->commit.notify = lock_surface_commit;
	wl_signal_add(&lock_surface->surface->events.commit, &lsl->commit);

	lock_surface_configure(lsl);
	s->keyboard_focus = lock_surface->surface;
	wlr_seat_keyboard_notify_enter(s->seat, lock_surface->surface, NULL, 0, NULL);
}

static void lock_clear(struct Seraphim *s)
{
	s->lock = NULL;
	s->lock_tree = NULL;
	s->keyboard_focus = NULL;
	s->last_focused = -1;
	apply_world(s);
}

static void lock_unlock(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, lock_unlock);

	(void)data;
	wlr_log(WLR_INFO, "seraphim: session unlocked");
	if (s->lock)
		wlr_session_lock_v1_destroy(s->lock);
}

static void lock_destroy(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, lock_destroy);

	(void)data;
	wl_list_remove(&s->lock_new_surface.link);
	wl_list_remove(&s->lock_unlock.link);
	wl_list_remove(&s->lock_destroy.link);
	if (s->lock_tree)
		wlr_scene_node_destroy(&s->lock_tree->node);
	lock_clear(s);
}

static void server_new_lock(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_lock);
	struct wlr_session_lock_v1 *lock = static_cast<struct wlr_session_lock_v1 *>(data);

	if (s->lock) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}
	s->lock = lock;
	s->lock_tree = wlr_scene_tree_create(&s->scene->tree);
	if (!s->lock_tree) {
		s->lock = NULL;
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	s->lock_new_surface.notify = lock_new_surface;
	wl_signal_add(&lock->events.new_surface, &s->lock_new_surface);
	s->lock_unlock.notify = lock_unlock;
	wl_signal_add(&lock->events.unlock, &s->lock_unlock);
	s->lock_destroy.notify = lock_destroy;
	wl_signal_add(&lock->events.destroy, &s->lock_destroy);

	wlr_session_lock_v1_send_locked(lock);
	wlr_log(WLR_INFO, "seraphim: session locked");
}

static void output_configuration_sync(struct Seraphim *s)
{
	struct wlr_output_configuration_v1 *config;
	struct SeraphimOutput *so;

	if (!s->output_manager)
		return;
	config = wlr_output_configuration_v1_create();
	if (!config)
		return;
	wl_list_for_each(so, &s->outputs, link)
		wlr_output_configuration_head_v1_create(config, so->wlr_output);
	wlr_output_manager_v1_set_configuration(s->output_manager, config);
}

static void output_manager_apply(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, output_apply);
	struct wlr_output_configuration_v1 *config =
			static_cast<struct wlr_output_configuration_v1 *>(data);
	struct wlr_output_configuration_head_v1 *head;
	bool ok = true;

	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *output = head->state.output;
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		wlr_output_head_v1_state_apply(&head->state, &state);
		if (!output || !wlr_output_commit_state(output, &state))
			ok = false;
		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);
	update_view(s);
	output_configuration_sync(s);
}

static void activation_request(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, activation_request);
	struct wlr_xdg_activation_v1_request_activate_event *event =
			static_cast<struct wlr_xdg_activation_v1_request_activate_event *>(data);
	SeraphimWindow *sw;

	if (!event->surface)
		return;

	wl_list_for_each(sw, &s->windows, link) {
		struct wlr_surface *surface = NULL;

		if (sw->toplevel)
			surface = sw->toplevel->base->surface;
#if SERAPHIM_XWAYLAND
		else if (sw->xsurface)
			surface = sw->xsurface->surface;
#endif
		if (surface != event->surface || sw->id <= 0)
			continue;
		s->world.focus(sw->id);
		apply_world(s);
		return;
	}
}

static void cursor_request_set_shape(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, cursor_request_set_shape);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event =
			static_cast<struct wlr_cursor_shape_manager_v1_request_set_shape_event *>(data);
	struct wlr_seat_client *focused = s->seat->pointer_state.focused_client;

	if (focused && focused == event->seat_client && s->cursor_mgr)
		wlr_cursor_set_xcursor(s->cursor, s->cursor_mgr,
				wlr_cursor_shape_v1_name(event->shape));
}

static void constraint_focus_sync(struct Seraphim *s)
{
	struct wlr_pointer_constraint_v1 *c = s->active_constraint;
	struct wlr_surface *focused = s->seat->pointer_state.focused_surface;

	if (!c)
		return;
	if (c->surface == focused) {
		if (!s->constraint_activated) {
			s->constraint_activated = true;
			wlr_pointer_constraint_v1_send_activated(c);
		}
	} else if (s->constraint_activated) {
		s->constraint_activated = false;
		wlr_pointer_constraint_v1_send_deactivated(c);
	}
}

static void constraint_apply(struct Seraphim *s)
{
	struct wlr_pointer_constraint_v1 *c = s->active_constraint;
	SeraphimWindow *sw;
	struct wlr_surface *surface = NULL;
	double ox = 0, oy = 0;

	if (!c || !s->constraint_activated)
		return;
	if (c->type != WLR_POINTER_CONSTRAINT_V1_LOCKED)
		return;
	if (!c->current.cursor_hint.enabled)
		return;

	wl_list_for_each(sw, &s->windows, link) {
		if (sw->toplevel)
			surface = sw->toplevel->base->surface;
#if SERAPHIM_XWAYLAND
		else if (sw->xsurface)
			surface = sw->xsurface->surface;
#endif
		if (surface && surface == c->surface && sw->tree) {
			ox = sw->tree->node.x;
			oy = sw->tree->node.y;
			break;
		}
	}
	wlr_cursor_warp_closest(s->cursor, NULL, ox + c->current.cursor_hint.x,
			oy + c->current.cursor_hint.y);
}

static void constraint_destroy(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, constraint_destroy);

	(void)data;
	wl_list_remove(&s->constraint_destroy.link);
	s->active_constraint = NULL;
	s->constraint_activated = false;
}

static void constraint_new(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_constraint);
	struct wlr_pointer_constraint_v1 *constraint =
			static_cast<struct wlr_pointer_constraint_v1 *>(data);

	if (s->active_constraint == constraint)
		return;
	if (s->active_constraint) {
		struct wlr_pointer_constraint_v1 *old = s->active_constraint;

		wl_list_remove(&s->constraint_destroy.link);
		s->active_constraint = NULL;
		s->constraint_activated = false;
		wlr_pointer_constraint_v1_send_deactivated(old);
	}
	s->active_constraint = constraint;
	s->constraint_destroy.notify = constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &s->constraint_destroy);
	constraint_focus_sync(s);
}

static void relative_motion(struct Seraphim *s, uint32_t time_msec, double dx, double dy)
{
	if (!s->relative_pointer)
		return;
	wlr_relative_pointer_manager_v1_send_relative_motion(s->relative_pointer,
			s->seat, (uint64_t)time_msec * 1000, dx, dy, dx, dy);
}

struct SeraphimTextInput {
	struct Seraphim *server;
	struct wlr_text_input_v3 *text_input;
	struct wl_listener enable, commit, disable, destroy;
};

static void text_input_send_state(struct Seraphim *s)
{
	struct wlr_text_input_v3 *ti = s->focused_text_input;

	if (!s->input_method || !ti)
		return;
	if (ti->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
		wlr_input_method_v2_send_surrounding_text(s->input_method,
				ti->current.surrounding.text ? ti->current.surrounding.text : "",
				ti->current.surrounding.cursor, ti->current.surrounding.anchor);
	if (ti->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
		wlr_input_method_v2_send_content_type(s->input_method,
				ti->current.content_type.hint, ti->current.content_type.purpose);
	wlr_input_method_v2_send_text_change_cause(s->input_method,
			ti->current.text_change_cause);
	wlr_input_method_v2_send_done(s->input_method);
}

static void text_input_focus_surface(struct Seraphim *s, struct wlr_surface *surface)
{
	struct wlr_text_input_v3 *ti;
	struct wl_client *client = NULL;
	bool matched = false;

	if (!s->text_input)
		return;
	if (surface)
		client = wl_resource_get_client(surface->resource);

	wl_list_for_each(ti, &s->text_input->text_inputs, link) {
		struct wl_client *owner;

		if (ti->seat != s->seat)
			continue;
		owner = wl_resource_get_client(ti->resource);
		if (!surface || owner != client) {
			if (ti->focused_surface)
				wlr_text_input_v3_send_leave(ti);
			continue;
		}
		if (ti->focused_surface == surface) {
			matched = true;
			continue;
		}

		if (ti->focused_surface)
			wlr_text_input_v3_send_leave(ti);
		wlr_text_input_v3_send_enter(ti, surface);
		matched = true;
	}

	if (!matched)
		s->focused_text_input = NULL;
}

static void text_input_enable(struct wl_listener *listener, void *data)
{
	struct SeraphimTextInput *link = wl_container_of(listener, link, enable);
	struct Seraphim *s = link->server;

	(void)data;

	if (link->text_input->focused_surface)
		s->focused_text_input = link->text_input;
	if (s->input_method)
		wlr_input_method_v2_send_activate(s->input_method);
	text_input_send_state(s);
}

static void text_input_commit(struct wl_listener *listener, void *data)
{
	struct SeraphimTextInput *link = wl_container_of(listener, link, commit);

	(void)data;
	text_input_send_state(link->server);
}

static void text_input_disable(struct wl_listener *listener, void *data)
{
	struct SeraphimTextInput *link = wl_container_of(listener, link, disable);
	struct Seraphim *s = link->server;

	(void)data;
	if (s->focused_text_input == link->text_input)
		s->focused_text_input = NULL;
	if (s->input_method)
		wlr_input_method_v2_send_deactivate(s->input_method);
}

static void text_input_destroy(struct wl_listener *listener, void *data)
{
	struct SeraphimTextInput *link = wl_container_of(listener, link, destroy);
	struct Seraphim *s = link->server;

	(void)data;
	if (s->focused_text_input == link->text_input)
		s->focused_text_input = NULL;
	wl_list_remove(&link->enable.link);
	wl_list_remove(&link->commit.link);
	wl_list_remove(&link->disable.link);
	wl_list_remove(&link->destroy.link);
	free(link);
}

static void server_new_text_input(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_text_input);
	struct wlr_text_input_v3 *text_input =
			static_cast<struct wlr_text_input_v3 *>(data);
	struct SeraphimTextInput *link =
			static_cast<struct SeraphimTextInput *>(calloc(1, sizeof(*link)));

	if (!link)
		return;
	link->server = s;
	link->text_input = text_input;
	link->enable.notify = text_input_enable;
	wl_signal_add(&text_input->events.enable, &link->enable);
	link->commit.notify = text_input_commit;
	wl_signal_add(&text_input->events.commit, &link->commit);
	link->disable.notify = text_input_disable;
	wl_signal_add(&text_input->events.disable, &link->disable);
	link->destroy.notify = text_input_destroy;
	wl_signal_add(&text_input->events.destroy, &link->destroy);
}

static void input_method_commit(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, input_method_commit);
	struct wlr_input_method_v2 *im = s->input_method;
	struct wlr_text_input_v3 *ti = s->focused_text_input;

	(void)data;
	if (!im || !ti || !ti->focused_surface)
		return;
	if (im->pending.preedit.text)
		wlr_text_input_v3_send_preedit_string(ti, im->pending.preedit.text,
				im->pending.preedit.cursor_begin,
				im->pending.preedit.cursor_end);
	if (im->pending.delete_.before_length || im->pending.delete_.after_length)
		wlr_text_input_v3_send_delete_surrounding_text(ti,
				im->pending.delete_.before_length,
				im->pending.delete_.after_length);
	if (im->pending.commit_text)
		wlr_text_input_v3_send_commit_string(ti, im->pending.commit_text);
	wlr_text_input_v3_send_done(ti);
}

static void input_method_destroy(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, input_method_destroy);

	(void)data;
	wl_list_remove(&s->input_method_commit.link);
	wl_list_remove(&s->input_method_destroy.link);
	s->input_method = NULL;
}

static void input_method_new(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_input_method);
	struct wlr_input_method_v2 *im = static_cast<struct wlr_input_method_v2 *>(data);

	if (s->input_method) {
		wlr_input_method_v2_send_unavailable(im);
		return;
	}
	s->input_method = im;
	s->input_method_commit.notify = input_method_commit;
	wl_signal_add(&im->events.commit, &s->input_method_commit);
	s->input_method_destroy.notify = input_method_destroy;
	wl_signal_add(&im->events.destroy, &s->input_method_destroy);
}

static void virtual_pointer_new(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_virtual_pointer);
	struct wlr_virtual_pointer_v1_new_pointer_event *event =
			static_cast<struct wlr_virtual_pointer_v1_new_pointer_event *>(data);
	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;

	wlr_cursor_attach_input_device(s->cursor, &pointer->pointer.base);
}

static void virtual_keyboard_new(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_virtual_keyboard);
	struct wlr_virtual_keyboard_v1 *keyboard =
			static_cast<struct wlr_virtual_keyboard_v1 *>(data);

	add_keyboard(s, &keyboard->keyboard);
}

#if SERAPHIM_XWAYLAND
static void x11_map(struct wl_listener *listener, void *data);
static void x11_unmap(struct wl_listener *listener, void *data);

static void x11_sync_props(struct SeraphimWindow *sw)
{
	foreign_sync(sw);
}

static void x11_associate(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, x_associate);
	struct Seraphim *s = sw->server;
	struct wlr_xwayland_surface *xs = sw->xsurface;
	int i;

	(void)data;
	if (!xs || !xs->surface || sw->tree)
		return;

	if (sw->override_redirect) {
		sw->tree = wlr_scene_tree_create(s->layers[LAYER_LS_OVERLAY]);
		if (sw->tree)
			wlr_scene_surface_create(sw->tree, xs->surface);
	} else {
		sw->tree = wlr_scene_tree_create(s->layers[LAYER_WINDOWS]);
		if (!sw->tree)
			return;
		wlr_scene_surface_create(sw->tree, xs->surface);
		for (i = 0; i < 4; i++) {
			sw->inner[i] = wlr_scene_rect_create(sw->tree, 1, 1,
					unfocused_inner_border_color);
			sw->outer[i] = wlr_scene_rect_create(sw->tree, 1, 1,
					unfocused_outer_border_color);
		}
	}

	sw->map.notify = x11_map;
	wl_signal_add(&xs->surface->events.map, &sw->map);
	sw->unmap.notify = x11_unmap;
	wl_signal_add(&xs->surface->events.unmap, &sw->unmap);
	sw->surface_hooks = true;

	if (xs->surface->mapped)
		x11_map(&sw->map, NULL);
}

static void x11_dissociate(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, x_dissociate);

	(void)data;
	if (!sw->surface_hooks)
		return;
	wl_list_remove(&sw->map.link);
	wl_list_remove(&sw->unmap.link);
	sw->surface_hooks = false;
}

static void x11_map(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, map);
	struct Seraphim *s = sw->server;
	struct wlr_xwayland_surface *xs = sw->xsurface;
	Rect geom;

	(void)data;
	if (!xs || !xs->surface || !sw->tree)
		return;

	if (sw->override_redirect) {
		wlr_scene_node_set_position(&sw->tree->node, xs->x, xs->y);
		wlr_scene_node_set_enabled(&sw->tree->node, true);
		return;
	}
	if (sw->mapped && sw->id > 0)
		return;

	if (!world_take_pending_geometry(&s->world, &geom))
		geom = world_default_geometry(&s->world);
	sw->id = s->world.add(geom, sw);
	sw->mapped = true;
	sw->want_w = (int)llround(geom.w);
	sw->want_h = (int)llround(geom.h);
	wlr_xwayland_surface_configure(xs, xs->x, xs->y, (uint16_t)sw->want_w,
			(uint16_t)sw->want_h);
	wlr_log(WLR_INFO, "seraphim: x11 window %d up at %g,%g %gx%g", sw->id,
			geom.x, geom.y, geom.w, geom.h);
	s->world.focus(sw->id);
	s->script.call_window_open(sw->id);
	apply_world(s);
	x11_sync_props(sw);
}

static void x11_unmap(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, unmap);
	struct Seraphim *s = sw->server;
	int id = sw->id;

	(void)data;
	if (sw->tree)
		wlr_scene_node_set_enabled(&sw->tree->node, false);
	sw->mapped = false;
	sw->id = 0;
	if (id > 0) {
		s->world.remove(id);
		s->script.call_window_close(id);
		apply_world(s);
	}
	x11_sync_props(sw);
}

static void x11_destroy(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, destroy);
	struct Seraphim *s = sw->server;
	int id = sw->id;

	(void)data;
	if (id > 0) {
		s->world.remove(id);
		s->script.call_window_close(id);
		apply_world(s);
	}
	if (sw->ftl) {
		struct wlr_foreign_toplevel_handle_v1 *handle = sw->ftl;

		sw->ftl = NULL;
		wlr_foreign_toplevel_handle_v1_destroy(handle);
	}
	if (sw->tree)
		wlr_scene_node_destroy(&sw->tree->node);
	if (sw->surface_hooks) {
		wl_list_remove(&sw->map.link);
		wl_list_remove(&sw->unmap.link);
	}
	wl_list_remove(&sw->x_associate.link);
	wl_list_remove(&sw->x_dissociate.link);
	wl_list_remove(&sw->destroy.link);
	wl_list_remove(&sw->x_request_configure.link);
	wl_list_remove(&sw->x_title.link);
	wl_list_remove(&sw->x_class.link);
	wl_list_remove(&sw->link);
	wlr_log(WLR_INFO, "seraphim: x11 window %d gone", id);
	free(sw);
}

static void x11_request_configure(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, x_request_configure);
	struct Seraphim *s = sw->server;
	struct wlr_xwayland_surface_configure_event *event =
			static_cast<struct wlr_xwayland_surface_configure_event *>(data);
	Window *win;

	if (sw->id <= 0)
		return;
	win = s->world.find(sw->id);
	if (!win)
		return;

	if (event->width > 0 && event->height > 0) {
		win->geom.w = event->width;
		win->geom.h = event->height;
		apply_world(s);
	}
}

static void x11_title_changed(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, x_title);

	(void)data;
	x11_sync_props(sw);
}

static void x11_class_changed(struct wl_listener *listener, void *data)
{
	SeraphimWindow *sw = wl_container_of(listener, sw, x_class);

	(void)data;
	x11_sync_props(sw);
}

static void xwayland_new_surface(struct wl_listener *listener, void *data)
{
	struct Seraphim *s = wl_container_of(listener, s, new_xwayland_surface);
	struct wlr_xwayland_surface *xs = static_cast<struct wlr_xwayland_surface *>(data);
	SeraphimWindow *sw = static_cast<SeraphimWindow *>(calloc(1, sizeof(*sw)));

	if (!sw)
		return;
	sw->server = s;
	sw->kind = WIN_X11;
	sw->xsurface = xs;
	sw->override_redirect = xs->override_redirect;

	sw->x_associate.notify = x11_associate;
	wl_signal_add(&xs->events.associate, &sw->x_associate);
	sw->x_dissociate.notify = x11_dissociate;
	wl_signal_add(&xs->events.dissociate, &sw->x_dissociate);
	sw->destroy.notify = x11_destroy;
	wl_signal_add(&xs->events.destroy, &sw->destroy);
	sw->x_request_configure.notify = x11_request_configure;
	wl_signal_add(&xs->events.request_configure, &sw->x_request_configure);
	sw->x_title.notify = x11_title_changed;
	wl_signal_add(&xs->events.set_title, &sw->x_title);
	sw->x_class.notify = x11_class_changed;
	wl_signal_add(&xs->events.set_class, &sw->x_class);

	wl_list_insert(&s->windows, &sw->link);
}
#endif

static void region_create(struct Seraphim *s)
{
#if SPAWNREGION_STYLE == 1
	s->region_fill = wlr_scene_rect_create(s->layers[LAYER_REGION], 1, 1,
			spawnregion_color);
	if (s->region_fill)
		wlr_scene_node_set_enabled(&s->region_fill->node, false);
#elif SPAWNREGION_STYLE == 2
	for (int i = 0; i < 4; i++) {
		s->region_border[i] = wlr_scene_rect_create(s->layers[LAYER_REGION], 1, 1,
				spawnregion_inner_border_color);
		s->region_border[4 + i] = wlr_scene_rect_create(s->layers[LAYER_REGION], 1, 1,
				spawnregion_outer_border_color);
		if (s->region_border[i])
			wlr_scene_node_set_enabled(&s->region_border[i]->node, false);
		if (s->region_border[4 + i])
			wlr_scene_node_set_enabled(&s->region_border[4 + i]->node, false);
	}
#else
	(void)s;
#endif
}

static int ipc_accept(int fd, uint32_t mask, void *data)
{
	struct Seraphim *s = static_cast<struct Seraphim *>(data);
	struct timeval tv;
	char buf[4096];
	size_t n = 0, sent = 0;
	std::string line, reply;
	int client;
	char *nl;

	(void)mask;
	client = accept(fd, NULL, NULL);
	if (client < 0)
		return 0;
	fcntl(client, F_SETFD, FD_CLOEXEC);
	tv.tv_sec = 0;
	tv.tv_usec = 200000;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	while (n < sizeof(buf) - 1) {
		ssize_t r = read(client, buf + n, sizeof(buf) - 1 - n);

		if (r <= 0)
			break;
		n += (size_t)r;
		if (memchr(buf, '\n', n))
			break;
	}
	buf[n] = '\0';
	nl = strchr(buf, '\n');
	if (nl)
		*nl = '\0';
	line = buf;

	reply = ipc_execute(&s->world, &s->script, line);
	apply_world(s);

	while (sent < reply.size()) {
		ssize_t w = write(client, reply.data() + sent, reply.size() - sent);

		if (w <= 0)
			break;
		sent += (size_t)w;
	}
	close(client);
	return 0;
}

static int handle_usr1(int sig, void *data)
{
	struct Seraphim *s = static_cast<struct Seraphim *>(data);

	(void)sig;
	s->world.reload_request = true;
	apply_world(s);
	return 0;
}

static void ipc_start(struct Seraphim *s)
{
	struct sockaddr_un addr;
	const char *rt = getenv("XDG_RUNTIME_DIR");
	int fd, probe;

	s->ipc_fd = -1;
	if (rt && *rt)
		snprintf(s->ipc_path, sizeof(s->ipc_path), "%s/seraphim.sock", rt);
	else
		snprintf(s->ipc_path, sizeof(s->ipc_path), "/tmp/seraphim-%d.sock", (int)getuid());

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlen(s->ipc_path) >= sizeof(addr.sun_path)) {
		wlr_log(WLR_ERROR, "seraphim: socket path too long, no IPC");
		return;
	}
	strcpy(addr.sun_path, s->ipc_path);

	probe = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (probe >= 0) {
		bool live = connect(probe, (struct sockaddr *)&addr, sizeof(addr)) == 0;

		close(probe);
		if (live) {
			wlr_log(WLR_ERROR, "seraphim: %s is in use by another seraphim, no IPC",
					s->ipc_path);
			return;
		}
	}
	unlink(s->ipc_path);

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
			listen(fd, 8) < 0) {
		wlr_log(WLR_ERROR, "seraphim: cannot open %s: %s, no IPC", s->ipc_path,
				strerror(errno));
		if (fd >= 0)
			close(fd);
		return;
	}
	chmod(s->ipc_path, 0600);

	s->ipc_fd = fd;
	s->ipc_source = wl_event_loop_add_fd(wl_display_get_event_loop(s->display), fd,
			WL_EVENT_READABLE, ipc_accept, s);
	setenv("SERAPHIM_SOCKET", s->ipc_path, 1);
	wlr_log(WLR_INFO, "seraphim: control socket %s", s->ipc_path);
}

static void ipc_stop(struct Seraphim *s)
{
	if (s->ipc_source)
		wl_event_source_remove(s->ipc_source);
	s->ipc_source = NULL;
	if (s->ipc_fd >= 0) {
		close(s->ipc_fd);
		unlink(s->ipc_path);
	}
	s->ipc_fd = -1;
	if (s->usr1_source)
		wl_event_source_remove(s->usr1_source);
	s->usr1_source = NULL;
}

int main(int argc, char *argv[])
{
	struct Seraphim s{};
	const char *socket;
	std::string script_path;
	int i;

	if (argc > 1 && strcmp(argv[1], "--version") == 0) {
		printf("seraphim %s (wlroots %s)\n", SERAPHIM_VERSION, WLR_PC);
		return 0;
	}

	wlr_log_init(WLR_INFO, NULL);
	install_signal_handling();
	install_crash_handler();

	g_display = s.display;
	signal(SIGINT, terminate_handler);
	signal(SIGTERM, terminate_handler);
	signal(SIGHUP, terminate_handler);

	wl_list_init(&s.outputs);
	wl_list_init(&s.windows);
	wl_list_init(&s.keyboards);

	s.display = wl_display_create();
	if (!s.display) {
		wlr_log(WLR_ERROR, "seraphim: cannot create a Wayland display");
		return 1;
	}
	s.event_loop = wl_display_get_event_loop(s.display);

	s.backend = wlr_backend_autocreate(s.event_loop, &s.session);
	if (!s.backend) {
		wlr_log(WLR_ERROR, "seraphim: cannot create a backend -- run me from "
				"a tty or a session, not inside another compositor");
		return 1;
	}
	s.renderer = wlr_renderer_autocreate(s.backend);
	if (!s.renderer || !wlr_renderer_init_wl_display(s.renderer, s.display)) {
		wlr_log(WLR_ERROR, "seraphim: cannot create a renderer");
		return 1;
	}
	s.allocator = wlr_allocator_autocreate(s.backend, s.renderer);
	if (!s.allocator) {
		wlr_log(WLR_ERROR, "seraphim: cannot create an allocator");
		return 1;
	}

	s.compositor = wlr_compositor_create(s.display, 6, s.renderer);
	wlr_subcompositor_create(s.display);
	wlr_data_device_manager_create(s.display);

	s.output_layout = wlr_output_layout_create(s.display);
	s.scene = wlr_scene_create();
	for (i = 0; i < LAYER_COUNT; i++)
		s.layers[i] = wlr_scene_tree_create(&s.scene->tree);
	s.background = wlr_scene_rect_create(s.layers[LAYER_BACKGROUND], 1, 1, rootcolor);
	region_create(&s);

	s.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(s.cursor, s.output_layout);
	s.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(s.cursor_mgr, 1);
	wlr_cursor_set_xcursor(s.cursor, s.cursor_mgr, "default");

	s.seat = wlr_seat_create(s.display, "seat0");
	s.xdg_shell = wlr_xdg_shell_create(s.display, 6);

	s.xdg_decoration = wlr_xdg_decoration_manager_v1_create(s.display);
	if (!s.xdg_decoration)
		wlr_log(WLR_ERROR, "seraphim: cannot create the decoration manager");
	else {
		s.new_xdg_decoration.notify = server_new_xdg_decoration;
		wl_signal_add(&s.xdg_decoration->events.new_toplevel_decoration,
				&s.new_xdg_decoration);
	}

	s.layer_shell = wlr_layer_shell_v1_create(s.display, 4);
	if (s.layer_shell) {
		s.new_layer_surface.notify = server_new_layer_surface;
		wl_signal_add(&s.layer_shell->events.new_surface, &s.new_layer_surface);
	} else {
		wlr_log(WLR_ERROR, "seraphim: no layer-shell -- bars, notifications, "
				"wallpapers and launchers will not start");
	}

	s.session_lock_mgr = wlr_session_lock_manager_v1_create(s.display);
	if (s.session_lock_mgr) {
		s.new_lock.notify = server_new_lock;
		wl_signal_add(&s.session_lock_mgr->events.new_lock, &s.new_lock);
	}

	s.foreign_toplevel = wlr_foreign_toplevel_manager_v1_create(s.display);

	s.cursor_shape = wlr_cursor_shape_manager_v1_create(s.display, 2);
	if (s.cursor_shape) {
		s.cursor_request_set_shape.notify = cursor_request_set_shape;
		wl_signal_add(&s.cursor_shape->events.request_set_shape,
				&s.cursor_request_set_shape);
	}

	s.activation = wlr_xdg_activation_v1_create(s.display);
	if (s.activation) {
		s.activation_request.notify = activation_request;
		wl_signal_add(&s.activation->events.request_activate,
				&s.activation_request);
	}

	s.output_manager = wlr_output_manager_v1_create(s.display);
	if (s.output_manager) {
		s.output_apply.notify = output_manager_apply;
		wl_signal_add(&s.output_manager->events.apply, &s.output_apply);
	}
	wlr_xdg_output_manager_v1_create(s.display, s.output_layout);
	s.gamma = wlr_gamma_control_manager_v1_create(s.display);

	s.screencopy = wlr_screencopy_manager_v1_create(s.display);
	s.ext_capture = wlr_ext_image_copy_capture_manager_v1_create(s.display, 1);

	wlr_primary_selection_v1_device_manager_create(s.display);
	wlr_data_control_manager_v1_create(s.display);
	wlr_ext_data_control_manager_v1_create(s.display, 1);

	wlr_viewporter_create(s.display);
	wlr_fractional_scale_manager_v1_create(s.display, 1);
	wlr_single_pixel_buffer_manager_v1_create(s.display);
	wlr_presentation_create(s.display, s.backend, 1);

	s.idle = wlr_idle_notifier_v1_create(s.display);
	s.idle_inhibit = wlr_idle_inhibit_v1_create(s.display);
	if (s.idle_inhibit) {
		s.new_idle_inhibitor.notify = idle_inhibitor_new;
		wl_signal_add(&s.idle_inhibit->events.new_inhibitor,
				&s.new_idle_inhibitor);
	}

	s.pointer_constraints = wlr_pointer_constraints_v1_create(s.display);
	if (s.pointer_constraints) {
		s.new_constraint.notify = constraint_new;
		wl_signal_add(&s.pointer_constraints->events.new_constraint,
				&s.new_constraint);
	}
	s.relative_pointer = wlr_relative_pointer_manager_v1_create(s.display);

	s.text_input = wlr_text_input_manager_v3_create(s.display);
	if (s.text_input) {
		s.new_text_input.notify = server_new_text_input;
		wl_signal_add(&s.text_input->events.new_text_input, &s.new_text_input);
	}
	s.input_method_mgr = wlr_input_method_manager_v2_create(s.display);
	if (s.input_method_mgr) {
		s.new_input_method.notify = input_method_new;
		wl_signal_add(&s.input_method_mgr->events.new_input_method,
				&s.new_input_method);
	}

	s.virtual_pointer = wlr_virtual_pointer_manager_v1_create(s.display);
	if (s.virtual_pointer) {
		s.new_virtual_pointer.notify = virtual_pointer_new;
		wl_signal_add(&s.virtual_pointer->events.new_virtual_pointer,
				&s.new_virtual_pointer);
	}
	s.virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(s.display);
	if (s.virtual_keyboard) {
		s.new_virtual_keyboard.notify = virtual_keyboard_new;
		wl_signal_add(&s.virtual_keyboard->events.new_virtual_keyboard,
				&s.new_virtual_keyboard);
	}

#if SERAPHIM_XWAYLAND
	s.xwayland = wlr_xwayland_create(s.display, s.compositor, true);
	if (s.xwayland) {
		s.xwayland->seat = s.seat;
		s.new_xwayland_surface.notify = xwayland_new_surface;
		wl_signal_add(&s.xwayland->events.new_surface,
				&s.new_xwayland_surface);
		if (s.xwayland->display_name) {
			setenv("DISPLAY", s.xwayland->display_name, true);
			wlr_log(WLR_INFO, "seraphim: xwayland on DISPLAY=%s",
					s.xwayland->display_name);
		}
	} else {
		wlr_log(WLR_INFO, "seraphim: no XWayland in this wlroots build");
	}
#endif

	s.new_output.notify = server_new_output;
	wl_signal_add(&s.backend->events.new_output, &s.new_output);
	s.new_input.notify = server_new_input;
	wl_signal_add(&s.backend->events.new_input, &s.new_input);
	s.new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&s.xdg_shell->events.new_toplevel, &s.new_xdg_toplevel);
	s.new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&s.xdg_shell->events.new_popup, &s.new_xdg_popup);

	s.cursor_motion.notify = cursor_motion;
	wl_signal_add(&s.cursor->events.motion, &s.cursor_motion);
	s.cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&s.cursor->events.motion_absolute, &s.cursor_motion_absolute);
	s.cursor_button.notify = cursor_button;
	wl_signal_add(&s.cursor->events.button, &s.cursor_button);
	s.cursor_axis.notify = cursor_axis;
	wl_signal_add(&s.cursor->events.axis, &s.cursor_axis);

	s.request_set_cursor.notify = seat_request_set_cursor;
	wl_signal_add(&s.seat->events.request_set_cursor, &s.request_set_cursor);

	socket = wl_display_add_socket_auto(s.display);
	if (!socket) {
		wlr_log(WLR_ERROR, "seraphim: cannot create a Wayland socket");
		return 1;
	}

	if (!s.script.init(&s.world)) {
		wlr_log(WLR_ERROR, "seraphim: %s", s.script.error.c_str());
		return 1;
	}
	script_path = Script::find_script(argv[0]);
	if (script_path.empty() || !s.script.load_file(script_path)) {
		wlr_log(WLR_ERROR, "seraphim: cannot load the desk script (%s). Put "
				"seraphim.as in ~/.config/seraphim/ or set SERAPHIM_SCRIPT.",
				s.script.error.c_str());
		return 1;
	}
	wlr_log(WLR_INFO, "seraphim: desk script %s", script_path.c_str());

	if (!wlr_backend_start(s.backend)) {
		wlr_log(WLR_ERROR, "seraphim: cannot start the backend");
		return 1;
	}
	if (!s.script_started) {
		s.script_started = true;
		s.script.call_init();
	}

	output_configuration_sync(&s);

	setenv("WAYLAND_DISPLAY", socket, true);
	setenv("XDG_SESSION_TYPE", "wayland", true);
	ipc_start(&s);
	s.usr1_source = wl_event_loop_add_signal(wl_display_get_event_loop(s.display),
			SIGUSR1, handle_usr1, &s);
	wlr_log(WLR_INFO, "seraphim: running on WAYLAND_DISPLAY=%s", socket);

	wl_display_run(s.display);
	ipc_stop(&s);

	wlr_log(WLR_INFO, "seraphim: shutting down");
	g_shutting_down = true;
	wl_display_destroy_clients(s.display);
	s.script.shutdown();

#if SERAPHIM_XWAYLAND
	if (s.xwayland)
		wlr_xwayland_destroy(s.xwayland);
#endif
	wlr_scene_node_destroy(&s.scene->tree.node);

	wl_list_remove(&s.cursor_motion.link);
	wl_list_remove(&s.cursor_motion_absolute.link);
	wl_list_remove(&s.cursor_button.link);
	wl_list_remove(&s.cursor_axis.link);
	wl_list_remove(&s.request_set_cursor.link);
	wlr_xcursor_manager_destroy(s.cursor_mgr);
	wlr_cursor_destroy(s.cursor);
	wlr_allocator_destroy(s.allocator);
	wlr_renderer_destroy(s.renderer);
	wlr_backend_destroy(s.backend);
	wlr_output_layout_destroy(s.output_layout);
	wl_display_destroy(s.display);
	return 0;
}
