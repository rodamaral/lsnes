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
extern const char* lua_sysrc_script;

lua_state LS;

void push_keygroup_parameters(lua_state& L, const struct keygroup::parameters& p)
{
	L.newtable();
	L.pushstring("last_rawval");
	L.pushnumber(p.last_rawval);
	L.settable(-3);
	L.pushstring("cal_left");
	L.pushnumber(p.cal_left);
	L.settable(-3);
	L.pushstring("cal_center");
	L.pushnumber(p.cal_center);
	L.settable(-3);
	L.pushstring("cal_right");
	L.pushnumber(p.cal_right);
	L.settable(-3);
	L.pushstring("cal_tolerance");
	L.pushnumber(p.cal_tolerance);
	L.settable(-3);
	L.pushstring("ktype");
	switch(p.ktype) {
	case keygroup::KT_DISABLED:		L.pushstring("disabled");		break;
	case keygroup::KT_KEY:			L.pushstring("key");		break;
	case keygroup::KT_AXIS_PAIR:		L.pushstring("axis");		break;
	case keygroup::KT_AXIS_PAIR_INVERSE:	L.pushstring("axis-inverse");	break;
	case keygroup::KT_HAT:			L.pushstring("hat");		break;
	case keygroup::KT_MOUSE:		L.pushstring("mouse");		break;
	case keygroup::KT_PRESSURE_PM:		L.pushstring("pressure-pm");	break;
	case keygroup::KT_PRESSURE_P0:		L.pushstring("pressure-p0");	break;
	case keygroup::KT_PRESSURE_0M:		L.pushstring("pressure-0m");	break;
	case keygroup::KT_PRESSURE_0P:		L.pushstring("pressure-0p");	break;
	case keygroup::KT_PRESSURE_M0:		L.pushstring("pressure-m0");	break;
	case keygroup::KT_PRESSURE_MP:		L.pushstring("pressure-mp");	break;
	};
	L.settable(-3);
}

lua_render_context* lua_render_ctx = NULL;
controller_frame* lua_input_controllerdata = NULL;
bool lua_booted_flag = false;

namespace
{
	bool recursive_flag = false;
	const char* luareader_fragment = NULL;

	const char* read_lua_fragment(lua_State* LS, void* dummy, size_t* size)
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

	bool callback_exists(const char* name)
	{
		if(recursive_flag)
			return false;
		LS.getglobal(name);
		int t = LS.type(-1);
		if(t != LUA_TFUNCTION)
			LS.pop(1);
		return (t == LUA_TFUNCTION);
	}

	void push_string(lua_state& L, const std::string& s)
	{
		L.pushlstring(s.c_str(), s.length());
	}

	void push_boolean(lua_state& L, bool b)
	{
		L.pushboolean(b ? 1 : 0);
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
		int t = L.load(read_lua_fragment, NULL, "run_lua_fragment", "bt");
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
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
	}

	void do_eval_lua(lua_state& L, const std::string& c) throw(std::bad_alloc)
	{
		push_string(L, c);
		L.setglobal(TEMPORARY);
		luareader_fragment = eval_lua_lua;
		run_lua_fragment(L);
	}

	void do_run_lua(lua_state& L, const std::string& c) throw(std::bad_alloc)
	{
		push_string(L, c);
		L.setglobal(TEMPORARY);
		luareader_fragment = run_lua_lua;
		run_lua_fragment(L);
	}

	void run_lua_cb(lua_state& L, int args) throw()
	{
		recursive_flag = true;
		int r = L.pcall(args, 0, 0);
		recursive_flag = false;
		if(r == LUA_ERRRUN) {
			messages << "Error running Lua callback: " << L.tostring(-1)  << std::endl;
			L.pop(1);
		}
		if(r == LUA_ERRMEM) {
			messages << "Error running Lua callback: Out of memory" << std::endl;
			L.pop(1);
		}
		if(r == LUA_ERRERR) {
			messages << "Error running Lua callback: Double Fault???" << std::endl;
			L.pop(1);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
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

}

void lua_callback_do_paint(struct lua_render_context* ctx, bool non_synthetic) throw()
{
	if(!callback_exists("on_paint"))
		return;
	lua_render_ctx = ctx;
	push_boolean(LS, non_synthetic);
	run_lua_cb(LS, 1);
	lua_render_ctx = NULL;
}

void lua_callback_do_video(struct lua_render_context* ctx) throw()
{
	if(!callback_exists("on_video"))
		return;
	lua_render_ctx = ctx;
	run_lua_cb(LS, 0);
	lua_render_ctx = NULL;
}

void lua_callback_do_reset() throw()
{
	if(!callback_exists("on_reset"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_frame() throw()
{
	if(!callback_exists("on_frame"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_rewind() throw()
{
	if(!callback_exists("on_rewind"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_idle() throw()
{
	lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	if(!callback_exists("on_idle"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_timer() throw()
{
	lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;	
	if(!callback_exists("on_timer"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_frame_emulated() throw()
{
	if(!callback_exists("on_frame_emulated"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_do_readwrite() throw()
{
	if(!callback_exists("on_readwrite"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_startup() throw()
{
	lua_booted_flag = true;
	if(!callback_exists("on_startup"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_pre_load(const std::string& name) throw()
{
	if(!callback_exists("on_pre_load"))
		return;
	push_string(LS, name);
	run_lua_cb(LS, 1);
}

void lua_callback_err_load(const std::string& name) throw()
{
	if(!callback_exists("on_err_load"))
		return;
	push_string(LS, name);
	run_lua_cb(LS, 1);
}

void lua_callback_post_load(const std::string& name, bool was_state) throw()
{
	if(!callback_exists("on_post_load"))
		return;
	push_string(LS, name);
	push_boolean(LS, was_state);
	run_lua_cb(LS, 2);
}

void lua_callback_pre_save(const std::string& name, bool is_state) throw()
{
	if(!callback_exists("on_pre_save"))
		return;
	push_string(LS, name);
	push_boolean(LS, is_state);
	run_lua_cb(LS, 2);
}

void lua_callback_err_save(const std::string& name) throw()
{
	if(!callback_exists("on_err_save"))
		return;
	push_string(LS, name);
	run_lua_cb(LS, 1);
}

void lua_callback_post_save(const std::string& name, bool is_state) throw()
{
	if(!callback_exists("on_post_save"))
		return;
	push_string(LS, name);
	push_boolean(LS, is_state);
	run_lua_cb(LS, 2);
}

void lua_callback_do_input(controller_frame& data, bool subframe) throw()
{
	if(!callback_exists("on_input"))
		return;
	lua_input_controllerdata = &data;
	push_boolean(LS, subframe);
	run_lua_cb(LS, 1);
	lua_input_controllerdata = NULL;
}

void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw()
{
	if(!callback_exists("on_snoop"))
		return;
	LS.pushnumber(port);
	LS.pushnumber(controller);
	LS.pushnumber(index);
	LS.pushnumber(value);
	run_lua_cb(LS, 4);
}

namespace
{
	function_ptr_command<const std::string&> evaluate_lua(lsnes_cmd, "evaluate-lua", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			do_eval_lua(LS, args);
		});

	function_ptr_command<arg_filename> run_lua(lsnes_cmd, "run-lua", "Run Lua script in Lua VM",
		"Syntax: run-lua <file>\nRuns <file> in Lua VM.\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error)
		{
			do_run_lua(LS, args);
		});

	function_ptr_command<> reset_lua(lsnes_cmd, "reset-lua", "Reset the Lua VM",
		"Syntax: reset-lua\nReset the Lua VM.\n",
		[]() throw(std::bad_alloc, std::runtime_error)
		{
			LS.reset();
			luaL_openlibs(LS.handle());

			run_sysrc_lua(LS);
			copy_system_tables(LS);
			messages << "Lua VM reset" << std::endl;
		});

}

void lua_callback_quit() throw()
{
	if(!callback_exists("on_quit"))
		return;
	run_lua_cb(LS, 0);
}

void lua_callback_keyhook(const std::string& key, const struct keygroup::parameters& p) throw()
{
	if(!callback_exists("on_keyhook"))
		return;
	LS.pushstring(key.c_str());
	push_keygroup_parameters(LS, p);
	run_lua_cb(LS, 2);
}

void init_lua(bool soft) throw()
{
	char tmpkey;
	LS.set_oom_handler(OOM_panic);
	try {
		LS.reset();
	} catch(std::exception& e) {
		messages << "Can't initialize Lua." << std::endl;
		if(soft)
			return;
		fatal_error();	
	}
	LS.getglobal("print");
	luaL_openlibs(LS.handle());
	LS.setglobal("print");
	run_sysrc_lua(LS);
	copy_system_tables(LS);
}

void quit_lua() throw()
{
	LS.deinit();
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
}

void lua_callback_do_unsafe_rewind(const std::vector<char>& save, uint64_t secs, uint64_t ssecs, movie& mov, void* u)
{
	if(u) {
		lua_unsaferewind* u2 = reinterpret_cast<lua_obj_pin<lua_unsaferewind>*>(u)->object();
		//Load.
		try {
			if(callback_exists("on_pre_rewind"))
				run_lua_cb(LS, 0);
			mainloop_restore_state(u2->state, u2->secs, u2->ssecs);
			mov.fast_load(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			if(callback_exists("on_post_rewind"))
				run_lua_cb(LS, 0);
		} catch(...) {
			return;
		}
	} else {
		//Save
		if(callback_exists("on_set_rewind")) {
			lua_unsaferewind* u2 = lua_class<lua_unsaferewind>::create(LS);
			u2->state = save;
			u2->secs = secs,
			u2->ssecs = ssecs;
			mov.fast_save(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			run_lua_cb(LS, 1);
		}
		
	}
}

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;

DECLARE_LUACLASS(lua_unsaferewind, "UNSAFEREWIND");
