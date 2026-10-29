#include "ipc.h"

#include "canvas.h"
#include "script.h"
#include "world.h"

#include "config_input.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

#ifndef SERAPHIM_VERSION
#define SERAPHIM_VERSION "dev"
#endif

static std::string fail(const std::string &why)
{
	return "error: " + why + "\n";
}

static std::string fmt(const char *f, double a = 0, double b = 0, double c = 0, double d = 0)
{
	char buf[256];

	snprintf(buf, sizeof(buf), f, a, b, c, d);
	return buf;
}

static bool to_number(const std::string &s, double *out)
{
	char *end = nullptr;

	if (s.empty())
		return false;
	*out = strtod(s.c_str(), &end);
	return end && *end == '\0';
}

static bool to_id(World *w, const std::string &s, int *id)
{
	double v;

	if (!to_number(s, &v))
		return false;
	*id = (int)v;
	return w->find(*id) != nullptr;
}

static std::string one_line(const std::string &in)
{
	std::string out = in;

	for (size_t i = 0; i < out.size(); i++)
		if (out[i] == '\t' || out[i] == '\n' || out[i] == '\r')
			out[i] = ' ';
	return out;
}

static std::string words_after(const std::string &line, size_t n)
{
	size_t i = 0;

	while (n > 0) {
		while (i < line.size() && line[i] == ' ')
			i++;
		while (i < line.size() && line[i] != ' ')
			i++;
		n--;
	}
	while (i < line.size() && line[i] == ' ')
		i++;
	return line.substr(i);
}

static std::string run_script_function(World *w, Script *s, const std::string &fn,
		const std::string &arg)
{
	std::string why;
	std::string out;

	w->ipc_out.clear();
	if (!s->call_named(fn, arg, &why)) {
		w->ipc_out.clear();
		return fail(why);
	}
	out = w->ipc_out;
	w->ipc_out.clear();
	return out.empty() ? std::string("ok\n") : out;
}

static const char *help_text =
	"windows                     id x y w h focused app_id title (tab separated)\n"
	"focused                     id of the focused window, 0 for none\n"
	"camera                      scale and pan\n"
	"view                        the canvas rectangle the screen shows\n"
	"zoom <scale>                zoom about the middle of the screen\n"
	"pan <dx> <dy>               pan by screen pixels\n"
	"focus <id>                  focus a window\n"
	"close <id>                  ask a window to close\n"
	"move <id> <x> <y>           move a window (canvas coordinates)\n"
	"geom <id> <x> <y> <w> <h>   place and size a window\n"
	"spawn [command...]          open a window, or run a command\n"
	"bookmark list               saved camera positions\n"
	"bookmark save <1-9>         save the camera position\n"
	"bookmark goto <1-9>         jump to a saved position\n"
	"layout <name>               apply a layout to the windows on screen\n"
	"layouts                     list the layouts the script knows\n"
	"call <ipc_fn> [arg...]      run void ipc_fn(const string &in) in the script\n"
	"reload                      re-read the script file\n"
	"quit                        stop seraphim\n"
	"ping | version | help\n";

std::string ipc_execute(World *w, Script *s, const std::string &raw)
{
	std::string line = raw;
	std::vector<std::string> a;
	std::istringstream in;
	std::string word, cmd;

	while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r' ||
			line[line.size() - 1] == ' '))
		line.erase(line.size() - 1);

	in.str(line);
	while (in >> word)
		a.push_back(word);
	if (a.empty())
		return fail("empty command, try help");
	cmd = a[0];

	if (cmd == "help")
		return help_text;
	if (cmd == "ping")
		return "pong\n";
	if (cmd == "version")
		return std::string(SERAPHIM_VERSION) + "\n";

	if (cmd == "windows") {
		std::string out;

		for (int i = 0; i < w->count(); i++) {
			Window *win = w->at(i);

			out += fmt("%.0f", win->id) + " " + fmt("%.0f %.0f %.0f %.0f", win->geom.x,
					win->geom.y, win->geom.w, win->geom.h);
			out += win->id == w->focused_id ? " 1\t" : " 0\t";
			out += one_line(win->app_id) + "\t" + one_line(win->title) + "\n";
		}
		return out.empty() ? std::string("ok\n") : out;
	}

	if (cmd == "focused")
		return fmt("%.0f\n", w->focused_id);

	if (cmd == "camera")
		return fmt("scale %.4f pan %.2f %.2f\n", w->camera.scale, w->camera.pan_x,
				w->camera.pan_y);

	if (cmd == "view") {
		Rect r;

		camera_visible_rect(&w->camera, w->view_w, w->view_h, &r);
		return fmt("%.1f %.1f %.1f %.1f\n", r.x, r.y, r.w, r.h);
	}

	if (cmd == "zoom") {
		double v;

		if (a.size() != 2 || !to_number(a[1], &v) || v <= 0.0)
			return fail("usage: zoom <scale>");
		camera_set_scale_at(&w->camera, v, w->view_w / 2.0, w->view_h / 2.0);
		return fmt("scale %.4f\n", w->camera.scale);
	}

	if (cmd == "pan") {
		double dx, dy;

		if (a.size() != 3 || !to_number(a[1], &dx) || !to_number(a[2], &dy))
			return fail("usage: pan <dx> <dy>");
		camera_pan_by(&w->camera, dx, dy);
		return "ok\n";
	}

	if (cmd == "focus" || cmd == "close") {
		int id;

		if (a.size() != 2 || !to_id(w, a[1], &id))
			return fail("usage: " + cmd + " <window id>, see windows");
		if (cmd == "focus") {
			w->raise(id);
			w->focus(id);
		} else {
			world_request_close(w, id);
		}
		return "ok\n";
	}

	if (cmd == "move") {
		int id;
		double x, y;

		if (a.size() != 4 || !to_id(w, a[1], &id) || !to_number(a[2], &x) ||
				!to_number(a[3], &y))
			return fail("usage: move <id> <x> <y>");
		w->find(id)->geom.x = x;
		w->find(id)->geom.y = y;
		return "ok\n";
	}

	if (cmd == "geom") {
		int id;
		double x, y, ww, hh;

		if (a.size() != 6 || !to_id(w, a[1], &id) || !to_number(a[2], &x) ||
				!to_number(a[3], &y) || !to_number(a[4], &ww) || !to_number(a[5], &hh))
			return fail("usage: geom <id> <x> <y> <w> <h>");
		if (ww < (double)min_window_w)
			ww = (double)min_window_w;
		if (hh < (double)min_window_h)
			hh = (double)min_window_h;
		w->find(id)->geom.x = x;
		w->find(id)->geom.y = y;
		w->find(id)->geom.w = ww;
		w->find(id)->geom.h = hh;
		return "ok\n";
	}

	if (cmd == "spawn") {
		Rect none = {0, 0, 0, 0};

		if (a.size() == 1)
			world_spawn(w, none);
		else
			world_queue_spawn(w, words_after(line, 1));
		return "ok\n";
	}

	if (cmd == "bookmark") {
		double n;

		if (a.size() == 2 && a[1] == "list") {
			std::string out;

			for (int i = 0; i < BOOKMARK_SLOTS; i++) {
				if (!w->bookmarks[i].set)
					continue;
				out += fmt("%.0f: ", i) + fmt("scale %.4f pan %.2f %.2f\n", w->bookmarks[i].scale,
						w->bookmarks[i].pan_x, w->bookmarks[i].pan_y);
			}
			return out.empty() ? std::string("ok\n") : out;
		}
		if (a.size() == 3 && (a[1] == "save" || a[1] == "goto") && to_number(a[2], &n) &&
				n >= 1 && n <= 9) {
			if (a[1] == "save")
				world_bookmark_save(w, (int)n);
			else if (!world_bookmark_goto(w, (int)n))
				return fail("nothing saved in slot " + a[2]);
			return "ok\n";
		}
		return fail("usage: bookmark list | save <1-9> | goto <1-9>");
	}

	if (cmd == "layout") {
		if (a.size() != 2)
			return fail("usage: layout <name>, see layouts");
		return run_script_function(w, s, "ipc_layout", a[1]);
	}

	if (cmd == "layouts")
		return run_script_function(w, s, "ipc_layouts", "");

	if (cmd == "call") {
		if (a.size() < 2)
			return fail("usage: call <ipc_function> [argument]");
		return run_script_function(w, s, a[1], words_after(line, 2));
	}

	if (cmd == "reload") {
		if (!s->reload())
			return fail(s->error);
		return "ok\n";
	}

	if (cmd == "quit") {
		w->quit_request = true;
		return "ok\n";
	}

	return fail("unknown command \"" + cmd + "\", try help");
}
