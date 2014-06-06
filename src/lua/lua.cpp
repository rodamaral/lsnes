#include "core/command.hpp"
#include "library/globalwrap.hpp"
#include "library/keyboard.hpp"
#include "lua/internal.hpp"
#include "lua/lua.hpp"
#include "lua/unsaferewind.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"
#include "core/misc.hpp"

#include <map>
#include <cstring>
#include <string>
#include <iostream>
extern "C" {
#include <lualib.h>
}

extern const char* lua_sysrc_script;

lua::function_group lua_func_bit;
lua::function_group lua_func_misc;
lua::function_group lua_func_load;
lua::function_group lua_func_zip;

lua::class_group lua_class_callback;
lua::class_group lua_class_gui;
lua::class_group lua_class_bind;
lua::class_group lua_class_pure;
lua::class_group lua_class_movie;
lua::class_group lua_class_memory;
lua::class_group lua_class_fileio;

namespace
{
	void pushpair(lua::state& L, std::string key, double value)
	{
		L.pushstring(key.c_str());
		L.pushnumber(value);
		L.settable(-3);
	}

	void pushpair(lua::state& L, std::string key, std::string value)
	{
		L.pushstring(key.c_str());
		L.pushstring(value.c_str());
		L.settable(-3);
	}

	std::string get_mode_str(int mode)
	{
		if(mode < 0)
			return "disabled";
		else if(mode > 0)
			return "axis";
		return "pressure0+";
	}
}

void push_keygroup_parameters(lua::state& L, keyboard::key& p)
{
	keyboard::mouse_calibration p2;
	int mode;
	L.newtable();
	switch(p.get_type()) {
	case keyboard::KBD_KEYTYPE_KEY:
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "key");
		break;
	case keyboard::KBD_KEYTYPE_HAT:
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "hat");
		break;
	case keyboard::KBD_KEYTYPE_MOUSE:
		p2 = p.cast_mouse()->get_calibration();
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "mouse");
		break;
	case keyboard::KBD_KEYTYPE_AXIS:
		mode = p.cast_axis()->get_mode();
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", get_mode_str(mode));
		break;
	}
}


namespace
{
	int push_keygroup_parameters2(lua::state& L, keyboard::key* p)
	{
		push_keygroup_parameters(L, *p);
		return 1;
	}


	const char* read_lua_fragment(lua_State* L, void* fragment, size_t* size)
	{
		const char*& luareader_fragment = *reinterpret_cast<const char**>(fragment);
		if(luareader_fragment) {
			const char* ret = luareader_fragment;
			*size = strlen(luareader_fragment);
			luareader_fragment = NULL;
			return ret;
		} else {
			*size = 0;
			return NULL;
		}
	}

#define TEMPORARY "LUAINTERP_INTERNAL_COMMAND_TEMPORARY"

	const char* eval_sysrc_lua = "local fn = loadstring(" TEMPORARY ", \"<built-in>\"); if fn then fn(); else "
		"print2(\"Parse error in sysrc.lua script\"); end;";
	const char* eval_lua_lua = "local fn = loadstring(" TEMPORARY "); if fn then fn(); else print("
		"\"Parse error in Lua statement\"); end;";
	const char* run_lua_lua = "dofile(" TEMPORARY ");";

	int system_write_error(lua_State* L)
	{
		lua_pushstring(L, "_SYSTEM is write-protected");
		lua_error(L);
		return 0;
	}

	void copy_system_tables(lua::state& L)
	{
#if LUA_VERSION_NUM == 501
		L.pushvalue(LUA_GLOBALSINDEX);
#endif
#if LUA_VERSION_NUM == 502
		L.rawgeti(LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#endif
		L.newtable();
		L.pushnil();
		while(L.next(-3)) {
			//Stack: _SYSTEM, KEY, VALUE
			L.pushvalue(-2);
			L.pushvalue(-2);
			//Stack: _SYSTEM, KEY, VALUE, KEY, VALUE
			L.rawset(-5);
			//Stack: _SYSTEM, KEY, VALUE
			L.pop(1);
			//Stack: _SYSTEM, KEY
		}
		L.newtable();
		L.pushcfunction(system_write_error);
		L.setfield(-2, "__newindex");
		L.setmetatable(-2);
		L.setglobal("_SYSTEM");
	}
}

lua_state::lua_state(lua::state& _L, command::group& _command)
	: L(_L), command(_command)
{
	requests_repaint = false;
	requests_subframe_paint = false;
	render_ctx = NULL;
	input_controllerdata = NULL;

	idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	veto_flag = NULL;
	kill_frame = NULL;
	hscl = NULL;
	vscl = NULL;
	synchronous_paint_ctx = NULL;
	recursive_flag = false;
	luareader_fragment = NULL;

	on_paint = new lua::state::callback_list(L, "paint", "on_paint");
	on_video = new lua::state::callback_list(L, "video", "on_video");
	on_reset = new lua::state::callback_list(L, "reset", "on_reset");
	on_frame = new lua::state::callback_list(L, "frame", "on_frame");
	on_rewind = new lua::state::callback_list(L, "rewind", "on_rewind");
	on_idle = new lua::state::callback_list(L, "idle", "on_idle");
	on_timer = new lua::state::callback_list(L, "timer", "on_timer");
	on_frame_emulated = new lua::state::callback_list(L, "frame_emulated", "on_frame_emulated");
	on_readwrite = new lua::state::callback_list(L, "readwrite", "on_readwrite");
	on_startup = new lua::state::callback_list(L, "startup", "on_startup");
	on_pre_load = new lua::state::callback_list(L, "pre_load", "on_pre_load");
	on_post_load = new lua::state::callback_list(L, "post_load", "on_post_load");
	on_err_load = new lua::state::callback_list(L, "err_load", "on_err_load");
	on_pre_save = new lua::state::callback_list(L, "pre_save", "on_pre_save");
	on_post_save = new lua::state::callback_list(L, "post_save", "on_post_save");
	on_err_save = new lua::state::callback_list(L, "err_save", "on_err_save");
	on_input = new lua::state::callback_list(L, "input", "on_input");
	on_snoop = new lua::state::callback_list(L, "snoop", "on_snoop");
	on_snoop2 = new lua::state::callback_list(L, "snoop2", "on_snoop2");
	on_button = new lua::state::callback_list(L, "button", "on_button");
	on_quit = new lua::state::callback_list(L, "quit", "on_quit");
	on_keyhook = new lua::state::callback_list(L, "keyhook", "on_keyhook");
	on_movie_lost = new lua::state::callback_list(L, "movie_lost", "on_movie_lost");
	on_pre_rewind = new lua::state::callback_list(L, "pre_rewind", "on_pre_rewind");
	on_post_rewind = new lua::state::callback_list(L, "post_rewind", "on_post_rewind");
	on_set_rewind = new lua::state::callback_list(L, "set_rewind", "on_set_rewind");
	on_latch = new lua::state::callback_list(L, "latch", "on_latch");
}

lua_state::~lua_state()
{
	delete on_paint;
	delete on_video;
	delete on_reset;
	delete on_frame;
	delete on_rewind;
	delete on_idle;
	delete on_timer;
	delete on_frame_emulated;
	delete on_readwrite;
	delete on_startup;
	delete on_pre_load;
	delete on_post_load;
	delete on_err_load;
	delete on_pre_save;
	delete on_post_save;
	delete on_err_save;
	delete on_input;
	delete on_snoop;
	delete on_snoop2;
	delete on_button;
	delete on_quit;
	delete on_keyhook;
	delete on_movie_lost;
	delete on_pre_rewind;
	delete on_post_rewind;
	delete on_set_rewind;
	delete on_latch;
}

void lua_state::callback_do_paint(struct lua::render_context* ctx, bool non_synthetic) throw()
{
	run_synchronous_paint(ctx);
	run_callback(*on_paint, lua::state::store_tag(render_ctx, ctx), lua::state::boolean_tag(non_synthetic));
}

void lua_state::callback_do_video(struct lua::render_context* ctx, bool& _kill_frame, uint32_t& _hscl,
	uint32_t& _vscl) throw()
{
	run_callback(*on_video, lua::state::store_tag(render_ctx, ctx), lua::state::store_tag(kill_frame,
		&_kill_frame), lua::state::store_tag(hscl, &_hscl), lua::state::store_tag(vscl, &_vscl));
}

void lua_state::callback_do_reset() throw()
{
	run_callback(*on_reset);
}

void lua_state::callback_do_frame() throw()
{
	run_callback(*on_frame);
}

void lua_state::callback_do_rewind() throw()
{
	run_callback(*on_rewind);
}

void lua_state::callback_do_idle() throw()
{
	idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	run_callback(*on_idle);
}

void lua_state::callback_do_timer() throw()
{
	timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	run_callback(*on_timer);
}

void lua_state::callback_do_frame_emulated() throw()
{
	run_callback(*on_frame_emulated);
}

void lua_state::callback_do_readwrite() throw()
{
	run_callback(*on_readwrite);
}

void lua_state::callback_pre_load(const std::string& name) throw()
{
	run_callback(*on_pre_load, lua::state::string_tag(name));
}

void lua_state::callback_err_load(const std::string& name) throw()
{
	run_callback(*on_err_load, lua::state::string_tag(name));
}

void lua_state::callback_post_load(const std::string& name, bool was_state) throw()
{
	run_callback(*on_post_load, lua::state::string_tag(name), lua::state::boolean_tag(was_state));
}

void lua_state::callback_pre_save(const std::string& name, bool is_state) throw()
{
	run_callback(*on_pre_save, lua::state::string_tag(name), lua::state::boolean_tag(is_state));
}

void lua_state::callback_err_save(const std::string& name) throw()
{
	run_callback(*on_err_save, lua::state::string_tag(name));
}

void lua_state::callback_post_save(const std::string& name, bool is_state) throw()
{
	run_callback(*on_post_save, lua::state::string_tag(name), lua::state::boolean_tag(is_state));
}

void lua_state::callback_do_input(controller_frame& data, bool subframe) throw()
{
	run_callback(*on_input, lua::state::store_tag(input_controllerdata, &data),
		lua::state::boolean_tag(subframe));
}

void lua_state::callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw()
{
	if(run_callback(*on_snoop2, lua::state::numeric_tag(port), lua::state::numeric_tag(controller),
		lua::state::numeric_tag(index), lua::state::numeric_tag(value)))
		return;
	run_callback(*on_snoop, lua::state::numeric_tag(port), lua::state::numeric_tag(controller),
		lua::state::numeric_tag(index), lua::state::numeric_tag(value));
}

bool lua_state::callback_do_button(uint32_t port, uint32_t controller, uint32_t index, const char* type)
{
	bool flag = false;
	run_callback(*on_button, lua::state::store_tag(veto_flag, &flag), lua::state::numeric_tag(port),
		lua::state::numeric_tag(controller), lua::state::numeric_tag(index), lua::state::string_tag(type));
	return flag;
}

namespace
{
	command::fnptr<const std::string&> evaluate_lua(lsnes_cmds, "evaluate-lua", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			auto& core = CORE();
			core.lua2->do_eval_lua(args);
		});

	command::fnptr<const std::string&> evaluate_lua2(lsnes_cmds, "L", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			auto& core = CORE();
			core.lua2->do_eval_lua(args);
		});

	command::fnptr<command::arg_filename> run_lua(lsnes_cmds, "run-lua", "Run Lua script in Lua VM",
		"Syntax: run-lua <file>\nRuns <file> in Lua VM.\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error)
		{
			auto& core = CORE();
			core.lua2->do_run_lua(args);
		});

	command::fnptr<> reset_lua(lsnes_cmds, "reset-lua", "Reset the Lua VM",
		"Syntax: reset-lua\nReset the Lua VM.\n",
		[]() throw(std::bad_alloc, std::runtime_error)
		{
			auto& core = CORE();
			core.lua->reset();
			luaL_openlibs(core.lua->handle());

			core.lua2->run_sysrc_lua();
			copy_system_tables(*core.lua);
			messages << "Lua VM reset" << std::endl;
		});

	lua::_class<lua_unsaferewind> class_unsaferewind(lua_class_movie, "UNSAFEREWIND", {}, {
	}, &lua_unsaferewind::print);
}

void lua_state::callback_quit() throw()
{
	run_callback(*on_quit);
}

void lua_state::callback_keyhook(const std::string& key, keyboard::key& p) throw()
{
	run_callback(*on_keyhook, lua::state::string_tag(key), lua::state::fnptr_tag(push_keygroup_parameters2, &p));
}

void init_lua() throw()
{
	auto& core = lsnes_instance;
	core.lua->set_oom_handler(OOM_panic);
	try {
		core.lua->reset();
		core.lua->add_function_group(lua_func_bit);
		core.lua->add_function_group(lua_func_load);
		core.lua->add_function_group(lua_func_misc);
		core.lua->add_function_group(lua_func_zip);
		core.lua->add_class_group(lua_class_callback);
		core.lua->add_class_group(lua_class_gui);
		core.lua->add_class_group(lua_class_bind);
		core.lua->add_class_group(lua_class_pure);
		core.lua->add_class_group(lua_class_movie);
		core.lua->add_class_group(lua_class_memory);
		core.lua->add_class_group(lua_class_fileio);
	} catch(std::exception& e) {
		messages << "Can't initialize Lua." << std::endl;
		fatal_error();
	}
	luaL_openlibs(core.lua->handle());
	core.lua2->run_sysrc_lua();
	copy_system_tables(*core.lua);
}

void quit_lua() throw()
{
	lsnes_instance.lua->deinit();
}


#define LUA_TIMED_HOOK_IDLE 0
#define LUA_TIMED_HOOK_TIMER 1

uint64_t lua_state::timed_hook(int timer) throw()
{
	switch(timer) {
	case LUA_TIMED_HOOK_IDLE:
		return idle_hook_time;
	case LUA_TIMED_HOOK_TIMER:
		return timer_hook_time;
	}
	return 0;
}

void lua_state::callback_do_unsafe_rewind(const std::vector<char>& save, uint64_t secs, uint64_t ssecs, movie& mov,
	void* u)
{
	auto& core = CORE();
	if(u) {
		lua_unsaferewind* u2 = reinterpret_cast<lua::objpin<lua_unsaferewind>*>(u)->object();
		//Load.
		try {
			run_callback(*on_pre_rewind);
			run_callback(*on_movie_lost, "unsaferewind");
			mainloop_restore_state(u2->state, u2->secs, u2->ssecs);
			mov.fast_load(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			try { core.mlogic->get_mfile().host_memory = u2->hostmemory; } catch(...) {}
			run_callback(*on_post_rewind);
			delete reinterpret_cast<lua::objpin<lua_unsaferewind>*>(u);
		} catch(...) {
			return;
		}
	} else {
		//Save
		run_callback(*on_set_rewind, lua::state::fn_tag([&core, save, secs, ssecs, &mov](lua::state& L) ->
			int {
			lua_unsaferewind* u2 = lua::_class<lua_unsaferewind>::create(*core.lua);
			u2->state = save;
			u2->secs = secs,
			u2->ssecs = ssecs;
			u2->hostmemory = core.mlogic->get_mfile().host_memory;
			mov.fast_save(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			return 1;
		}));
	}
}

void lua_state::callback_movie_lost(const char* what)
{
	run_callback(*on_movie_lost, std::string(what));
}

void lua_state::callback_do_latch(std::list<std::string>& args)
{
	run_callback(*on_latch, lua::state::vararg_tag(args));
}

lua_unsaferewind::lua_unsaferewind(lua::state& L)
{
}

void lua_state::run_startup_scripts()
{
	for(auto i : startup_scripts) {
		messages << "Trying to run Lua script: " << i << std::endl;
		do_run_lua(i);
	}
}

void lua_state::add_startup_script(const std::string& file)
{
	startup_scripts.push_back(file);
}

const std::map<std::string, std::u32string>& lua_state::get_watch_vars()
{
	return watch_vars;
}

void lua_state::run_lua_fragment() throw(std::bad_alloc)
{
	if(recursive_flag)
		return;
#if LUA_VERSION_NUM == 501
	int t = L.load(read_lua_fragment, &luareader_fragment, "run_lua_fragment");
#endif
#if LUA_VERSION_NUM == 502
	int t = L.load(read_lua_fragment, &luareader_fragment, "run_lua_fragment", "t");
#endif
	if(t == LUA_ERRSYNTAX) {
		messages << "Can't run Lua: Internal syntax error: " << L.tostring(-1)
			<< std::endl;
		L.pop(1);
		return;
	}
	if(t == LUA_ERRMEM) {
		messages << "Can't run Lua: Out of memory" << std::endl;
		L.pop(1);
		return;
	}
	recursive_flag = true;
	int r = L.pcall(0, 0, 0);
	recursive_flag = false;
	if(r == LUA_ERRRUN) {
		messages << "Error running Lua hunk: " << L.tostring(-1)  << std::endl;
		L.pop(1);
	}
	if(r == LUA_ERRMEM) {
		messages << "Error running Lua hunk: Out of memory" << std::endl;
		L.pop(1);
	}
	if(r == LUA_ERRERR) {
		messages << "Error running Lua hunk: Double Fault???" << std::endl;
		L.pop(1);
	}
	render_ctx = NULL;
	if(requests_repaint) {
		requests_repaint = false;
		command.invoke("repaint");
	}
}

void lua_state::do_eval_lua(const std::string& c) throw(std::bad_alloc)
{
	L.pushlstring(c.c_str(), c.length());
	L.setglobal(TEMPORARY);
	luareader_fragment = eval_lua_lua;
	run_lua_fragment();
}

void lua_state::do_run_lua(const std::string& c) throw(std::bad_alloc)
{
	L.pushlstring(c.c_str(), c.length());
	L.setglobal(TEMPORARY);
	luareader_fragment = run_lua_lua;
	run_lua_fragment();
}

template<typename... T> bool lua_state::run_callback(lua::state::callback_list& list, T... args)
{
	if(recursive_flag)
		return true;
	recursive_flag = true;
	try {
		if(!list.callback(args...)) {
			recursive_flag = false;
			return false;
		}
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
	recursive_flag = false;
	render_ctx = NULL;
	if(requests_repaint) {
		requests_repaint = false;
		command.invoke("repaint");
	}
	return true;
}

void lua_state::run_sysrc_lua()
{
	L.pushstring(lua_sysrc_script);
	L.setglobal(TEMPORARY);
	luareader_fragment = eval_sysrc_lua;
	run_lua_fragment();
}

void lua_state::run_synchronous_paint(struct lua::render_context* ctx)
{
	if(!synchronous_paint_ctx)
		return;
	lua_renderq_run(ctx, synchronous_paint_ctx);
}
