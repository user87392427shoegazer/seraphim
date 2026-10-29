#ifndef SERAPHIM_SCRIPT_H
#define SERAPHIM_SCRIPT_H

#include <string>

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;

struct World;

struct Script {
	asIScriptEngine *engine = nullptr;
	asIScriptContext *ctx = nullptr;
	asIScriptModule *module = nullptr;
	World *world = nullptr;
	bool loaded = false;
	std::string path;
	int generation = 0;
	std::string error;

	asIScriptFunction *fn_init = nullptr;
	asIScriptFunction *fn_motion = nullptr;
	asIScriptFunction *fn_button = nullptr;
	asIScriptFunction *fn_axis = nullptr;
	asIScriptFunction *fn_key = nullptr;
	asIScriptFunction *fn_window_open = nullptr;
	asIScriptFunction *fn_window_close = nullptr;
	asIScriptFunction *fn_window_changed = nullptr;
	asIScriptFunction *fn_reload = nullptr;

	bool init(World *w);

	bool load_file(const std::string &path);
	bool load_source(const char *name, const char *code);
	void shutdown();

	bool call_init();

	void call_motion(float sx, float sy, int hit, unsigned mods);

	bool call_button(int button, bool pressed, float sx, float sy, int hit, unsigned mods);
	void call_axis(float delta, float sx, float sy, unsigned mods);

	bool call_key(unsigned keysym, unsigned mods, bool pressed);
	void call_window_open(int id);
	void call_window_close(int id);
	void call_window_changed(int id);
	void call_reload();

	bool reload();

	bool call_named(const std::string &name, const std::string &arg,
			std::string *why);

	static std::string find_script(const char *argv0 = nullptr);

private:
	bool register_api();
	bool resolve_entry_points();
	bool execute(const char *what);
};

#endif
