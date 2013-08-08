#include "core/command.hpp"
#include "library/globalwrap.hpp"
#include "lua/internal.hpp"
#include "lua/lua.hpp"
#include "lua/unsaferewind.hpp"
#include "core/mainloop.hpp"
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

uint64_t lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
uint64_t lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;
bool* lua_veto_flag = NULL;
bool* lua_kill_frame = NULL;
extern const char* lua_sysrc_script;

lua_state lsnes_lua_state;
lua_function_group lua_func_bit;
lua_function_group lua_func_misc;
lua_function_group lua_func_callback;
lua_function_group lua_func_load;

namespace
{
	void pushpair(lua_state& L, std::string key, double value)
	{
		L.pushstring(key.c_str());
		L.pushnumber(value);
		L.settable(-3);
	}

	void pushpair(lua_state& L, std::string key, std::string value)
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

void push_keygroup_parameters(lua_state& L, keyboard_key& p)
{
	keyboard_mouse_calibration p2;
	keyboard_axis_calibration p3;
	int mode;
	L.newtable();
	switch(p.get_type()) {
	case KBD_KEYTYPE_KEY:
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "key");
		break;
	case KBD_KEYTYPE_HAT:
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "hat");
		break;
	case KBD_KEYTYPE_MOUSE:
		p2 = p.cast_mouse()->get_calibration();
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", "mouse");
		break;
	case KBD_KEYTYPE_AXIS:
		mode = p.cast_axis()->get_mode();
		pushpair(L, "value", p.get_state());
		pushpair(L, "type", get_mode_str(mode));
		break;
	}
}

lua_render_context* lua_render_ctx = NULL;
controller_frame* lua_input_controllerdata = NULL;
bool lua_booted_flag = false;

namespace
{
	int push_keygroup_parameters2(lua_state& L, keyboard_key* p)
	{
		push_keygroup_parameters(L, *p);
		return 1;
	}

	bool recursive_flag = false;
	const char* luareader_fragment = NULL;

	const char* read_lua_fragment(lua_State* L, void* dummy, size_t* size)
	{
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

	const char* eval_lua_lua = "loadstring(" TEMPORARY ")();";
	const char* run_lua_lua = "dofile(" TEMPORARY ");";

	void run_lua_fragment(lua_state& L) throw(std::bad_alloc)
	{
		if(recursive_flag)
			return;
#if LUA_VERSION_NUM == 501
		int t = L.load(read_lua_fragment, NULL, "run_lua_fragment");
#endif
#if LUA_VERSION_NUM == 502
		int t = L.load(read_lua_fragment, NULL, "run_lua_fragment", "t");
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
		lua_render_ctx = NULL;
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
	}

	void do_eval_lua(lua_state& L, const std::string& c) throw(std::bad_alloc)
	{
		L.pushlstring(c.c_str(), c.length());
		L.setglobal(TEMPORARY);
		luareader_fragment = eval_lua_lua;
		run_lua_fragment(L);
	}

	void do_run_lua(lua_state& L, const std::string& c) throw(std::bad_alloc)
	{
		L.pushlstring(c.c_str(), c.length());
		L.setglobal(TEMPORARY);
		luareader_fragment = run_lua_lua;
		run_lua_fragment(L);
	}

	template<typename... T> bool run_callback(lua_state::lua_callback_list& list, T... args)
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
		lua_render_ctx = NULL;
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
		recursive_flag = false;
		return true;
	}

	int system_write_error(lua_State* L)
	{
		lua_pushstring(L, "_SYSTEM is write-protected");
		lua_error(L);
		return 0;
	}

	void copy_system_tables(lua_state& L)
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

	void run_sysrc_lua(lua_state& L)
	{
		do_eval_lua(L, lua_sysrc_script);
	}

#define DEFINE_CB(X) lua_state::lua_callback_list on_##X (lsnes_lua_state, #X , "on_" #X )

	DEFINE_CB(paint);
	DEFINE_CB(video);
	DEFINE_CB(reset);
	DEFINE_CB(frame);
	DEFINE_CB(rewind);
	DEFINE_CB(idle);
	DEFINE_CB(timer);
	DEFINE_CB(frame_emulated);
	DEFINE_CB(readwrite);
	DEFINE_CB(startup);
	DEFINE_CB(pre_load);
	DEFINE_CB(post_load);
	DEFINE_CB(err_load);
	DEFINE_CB(pre_save);
	DEFINE_CB(post_save);
	DEFINE_CB(err_save);
	DEFINE_CB(input);
	DEFINE_CB(snoop);
	DEFINE_CB(snoop2);
	DEFINE_CB(button);
	DEFINE_CB(quit);
	DEFINE_CB(keyhook);
	DEFINE_CB(movie_lost);
	DEFINE_CB(pre_rewind);
	DEFINE_CB(post_rewind);
	DEFINE_CB(set_rewind);
}

void lua_callback_do_paint(struct lua_render_context* ctx, bool non_synthetic) throw()
{
	run_callback(on_paint, lua_state::store_tag(lua_render_ctx, ctx), lua_state::boolean_tag(non_synthetic));
}

void lua_callback_do_video(struct lua_render_context* ctx, bool& kill_frame) throw()
{
	run_callback(on_video, lua_state::store_tag(lua_render_ctx, ctx), lua_state::store_tag(lua_kill_frame,
		&kill_frame));
}

void lua_callback_do_reset() throw()
{
	run_callback(on_reset);
}

void lua_callback_do_frame() throw()
{
	run_callback(on_frame);
}

void lua_callback_do_rewind() throw()
{
	run_callback(on_rewind);
}

void lua_callback_do_idle() throw()
{
	lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	run_callback(on_idle);
}

void lua_callback_do_timer() throw()
{
	lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	run_callback(on_timer);
}

void lua_callback_do_frame_emulated() throw()
{
	run_callback(on_frame_emulated);
}

void lua_callback_do_readwrite() throw()
{
	run_callback(on_readwrite);
}

void lua_callback_startup() throw()
{
	lua_booted_flag = true;
	run_callback(on_startup);
}

void lua_callback_pre_load(const std::string& name) throw()
{
	run_callback(on_pre_load, lua_state::string_tag(name));
}

void lua_callback_err_load(const std::string& name) throw()
{
	run_callback(on_err_load, lua_state::string_tag(name));
}

void lua_callback_post_load(const std::string& name, bool was_state) throw()
{
	run_callback(on_post_load, lua_state::string_tag(name), lua_state::boolean_tag(was_state));
}

void lua_callback_pre_save(const std::string& name, bool is_state) throw()
{
	run_callback(on_pre_save, lua_state::string_tag(name), lua_state::boolean_tag(is_state));
}

void lua_callback_err_save(const std::string& name) throw()
{
	run_callback(on_err_save, lua_state::string_tag(name));
}

void lua_callback_post_save(const std::string& name, bool is_state) throw()
{
	run_callback(on_post_save, lua_state::string_tag(name), lua_state::boolean_tag(is_state));
}

void lua_callback_do_input(controller_frame& data, bool subframe) throw()
{
	run_callback(on_input, lua_state::store_tag(lua_input_controllerdata, &data),
		lua_state::boolean_tag(subframe));
}

void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw()
{
	if(run_callback(on_snoop2, lua_state::numeric_tag(port), lua_state::numeric_tag(controller),
		lua_state::numeric_tag(index), lua_state::numeric_tag(value)))
		return;
	run_callback(on_snoop, lua_state::numeric_tag(port), lua_state::numeric_tag(controller),
		lua_state::numeric_tag(index), lua_state::numeric_tag(value));
}

bool lua_callback_do_button(uint32_t port, uint32_t controller, uint32_t index, const char* type)
{
	bool flag = false;
	run_callback(on_button, lua_state::store_tag(lua_veto_flag, &flag), lua_state::numeric_tag(port),
		lua_state::numeric_tag(controller), lua_state::numeric_tag(index), lua_state::string_tag(type));
	return flag;
}

namespace
{
	function_ptr_command<const std::string&> evaluate_lua(lsnes_cmd, "evaluate-lua", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			do_eval_lua(lsnes_lua_state, args);
		});

	function_ptr_command<const std::string&> evaluate_lua2(lsnes_cmd, "L", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			do_eval_lua(lsnes_lua_state, args);
		});

	function_ptr_command<arg_filename> run_lua(lsnes_cmd, "run-lua", "Run Lua script in Lua VM",
		"Syntax: run-lua <file>\nRuns <file> in Lua VM.\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error)
		{
			do_run_lua(lsnes_lua_state, args);
		});

	function_ptr_command<> reset_lua(lsnes_cmd, "reset-lua", "Reset the Lua VM",
		"Syntax: reset-lua\nReset the Lua VM.\n",
		[]() throw(std::bad_alloc, std::runtime_error)
		{
			lsnes_lua_state.reset();
			luaL_openlibs(lsnes_lua_state.handle());

			run_sysrc_lua(lsnes_lua_state);
			copy_system_tables(lsnes_lua_state);
			messages << "Lua VM reset" << std::endl;
		});

}

void lua_callback_quit() throw()
{
	run_callback(on_quit);
}

void lua_callback_keyhook(const std::string& key, keyboard_key& p) throw()
{
	run_callback(on_keyhook, lua_state::string_tag(key), lua_state::fnptr_tag(push_keygroup_parameters2, &p));
}

void init_lua() throw()
{
	lsnes_lua_state.set_oom_handler(OOM_panic);
	try {
		lsnes_lua_state.reset();
		lsnes_lua_state.add_function_group(lua_func_bit);
		lsnes_lua_state.add_function_group(lua_func_load);
		lsnes_lua_state.add_function_group(lua_func_callback);
		lsnes_lua_state.add_function_group(lua_func_misc);
	} catch(std::exception& e) {
		messages << "Can't initialize Lua." << std::endl;
		fatal_error();
	}
	luaL_openlibs(lsnes_lua_state.handle());
	run_sysrc_lua(lsnes_lua_state);
	copy_system_tables(lsnes_lua_state);
}

void quit_lua() throw()
{
	lsnes_lua_state.deinit();
}


#define LUA_TIMED_HOOK_IDLE 0
#define LUA_TIMED_HOOK_TIMER 1

uint64_t lua_timed_hook(int timer) throw()
{
	switch(timer) {
	case LUA_TIMED_HOOK_IDLE:
		return lua_idle_hook_time;
	case LUA_TIMED_HOOK_TIMER:
		return lua_timer_hook_time;
	}
	return 0;
}

void lua_callback_do_unsafe_rewind(const std::vector<char>& save, uint64_t secs, uint64_t ssecs, movie& mov, void* u)
{
	if(u) {
		lua_unsaferewind* u2 = reinterpret_cast<lua_obj_pin<lua_unsaferewind>*>(u)->object();
		//Load.
		try {
			run_callback(on_pre_rewind);
			run_callback(on_movie_lost, "unsaferewind");
			mainloop_restore_state(u2->state, u2->secs, u2->ssecs);
			mov.fast_load(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			try { get_host_memory() = u2->hostmemory; } catch(...) {}
			run_callback(on_post_rewind);
		} catch(...) {
			return;
		}
	} else {
		//Save
		run_callback(on_set_rewind, lua_state::fn_tag([save, secs, ssecs, &mov](lua_state& L) -> int {
			lua_unsaferewind* u2 = lua_class<lua_unsaferewind>::create(lsnes_lua_state);
			u2->state = save;
			u2->secs = secs,
			u2->ssecs = ssecs;
			u2->hostmemory = get_host_memory();
			mov.fast_save(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			return 1;
		}));
	}
}

void lua_callback_movie_lost(const char* what)
{
	run_callback(on_movie_lost, std::string(what));
}

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;

DECLARE_LUACLASS(lua_unsaferewind, "UNSAFEREWIND");
