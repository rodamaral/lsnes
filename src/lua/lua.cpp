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
#include <lua.h>
#include <lualib.h>
}

uint64_t lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
uint64_t lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;
extern const char* lua_sysrc_script;

namespace
{
	globalwrap<std::map<std::string, lua_function*>> functions;
	lua_State* lua_initialized;
	int lua_trampoline_function(lua_State* L)
	{
		void* ptr = lua_touserdata(L, lua_upvalueindex(1));
		lua_function* f = reinterpret_cast<lua_function*>(ptr);
		try {
			return f->invoke(L);
		} catch(std::exception& e) {
			lua_pushfstring(L, "Error in internal function: %s", e.what());
			lua_error(L);
		}
	}

	//Pushes given table to top of stack, creating if needed.
	void recursive_lookup_table(lua_State* L, const std::string& tab)
	{
		if(tab == "") {
			lua_getglobal(L, "_G");
			return;
		}
		std::string u = tab;
		size_t split = u.find_last_of(".");
		std::string u1;
		std::string u2 = u;
		if(split < u.length()) {
			u1 = u.substr(0, split);
			u2 = u.substr(split + 1);
		}
		recursive_lookup_table(L, u1);
		lua_getfield(L, -1, u2.c_str());
		if(lua_type(L, -1) != LUA_TTABLE) {
			//Not a table, create a table.
			lua_pop(L, 1);
			lua_newtable(L);
			lua_setfield(L, -2, u2.c_str());
			lua_getfield(L, -1, u2.c_str());
		}
		//Get rid of previous table.
		lua_insert(L, -2);
		lua_pop(L, 1);
	}

	void register_lua_function(lua_State* L, const std::string& fun)
	{
		std::string u = fun;
		size_t split = u.find_last_of(".");
		std::string u1;
		std::string u2 = u;
		if(split < u.length()) {
			u1 = u.substr(0, split);
			u2 = u.substr(split + 1);
		}
		recursive_lookup_table(L, u1);
		void* ptr = reinterpret_cast<void*>(functions()[fun]);
		lua_pushlightuserdata(L, ptr);
		lua_pushcclosure(L, lua_trampoline_function, 1);
		lua_setfield(L, -2, u2.c_str());
		lua_pop(L, 1);
	}

	void register_lua_functions(lua_State* L)
	{
		for(auto i : functions())
			register_lua_function(L, i.first);
		lua_initialized = L;
	}
}

lua_function::lua_function(const std::string& name) throw(std::bad_alloc)
{
	functions()[fname = name] = this;
	if(lua_initialized)
		register_lua_function(lua_initialized, fname);
}

lua_function::~lua_function() throw()
{
	functions().erase(fname);
}

std::string get_string_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex)) {
		char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be string", argindex, fname);
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}
	size_t len;
	const char* f = lua_tolstring(LS, argindex, &len);
	if(!f) {
		char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be string", argindex, fname);
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}
	return std::string(f, f + len);
}

bool get_boolean_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex) || !lua_isboolean(LS, argindex)) {
		char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be boolean", argindex, fname);
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}
	return (lua_toboolean(LS, argindex) != 0);
}

void push_keygroup_parameters(lua_State* LS, const struct keygroup::parameters& p)
{
	lua_newtable(LS);
	lua_pushstring(LS, "last_rawval");
	lua_pushnumber(LS, p.last_rawval);
	lua_settable(LS, -3);
	lua_pushstring(LS, "cal_left");
	lua_pushnumber(LS, p.cal_left);
	lua_settable(LS, -3);
	lua_pushstring(LS, "cal_center");
	lua_pushnumber(LS, p.cal_center);
	lua_settable(LS, -3);
	lua_pushstring(LS, "cal_right");
	lua_pushnumber(LS, p.cal_right);
	lua_settable(LS, -3);
	lua_pushstring(LS, "cal_tolerance");
	lua_pushnumber(LS, p.cal_tolerance);
	lua_settable(LS, -3);
	lua_pushstring(LS, "ktype");
	switch(p.ktype) {
	case keygroup::KT_DISABLED:		lua_pushstring(LS, "disabled");		break;
	case keygroup::KT_KEY:			lua_pushstring(LS, "key");		break;
	case keygroup::KT_AXIS_PAIR:		lua_pushstring(LS, "axis");		break;
	case keygroup::KT_AXIS_PAIR_INVERSE:	lua_pushstring(LS, "axis-inverse");	break;
	case keygroup::KT_HAT:			lua_pushstring(LS, "hat");		break;
	case keygroup::KT_MOUSE:		lua_pushstring(LS, "mouse");		break;
	case keygroup::KT_PRESSURE_PM:		lua_pushstring(LS, "pressure-pm");	break;
	case keygroup::KT_PRESSURE_P0:		lua_pushstring(LS, "pressure-p0");	break;
	case keygroup::KT_PRESSURE_0M:		lua_pushstring(LS, "pressure-0m");	break;
	case keygroup::KT_PRESSURE_0P:		lua_pushstring(LS, "pressure-0p");	break;
	case keygroup::KT_PRESSURE_M0:		lua_pushstring(LS, "pressure-m0");	break;
	case keygroup::KT_PRESSURE_MP:		lua_pushstring(LS, "pressure-mp");	break;
	};
	lua_settable(LS, -3);
}

lua_render_context* lua_render_ctx = NULL;
controller_frame* lua_input_controllerdata = NULL;
bool lua_booted_flag = false;

namespace
{
	lua_State* L;
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

	void* alloc(void* user, void* old, size_t olds, size_t news)
	{
		if(news) {
			void* m = realloc(old, news);
			if(!m)
				OOM_panic();
			return m;
		} else
			free(old);
		return NULL;
	}

	bool callback_exists(const char* name)
	{
		if(recursive_flag)
			return false;
		lua_getglobal(L, name);
		int t = lua_type(L, -1);
		if(t != LUA_TFUNCTION)
			lua_pop(L, 1);
		return (t == LUA_TFUNCTION);
	}

	void push_string(const std::string& s)
	{
		lua_pushlstring(L, s.c_str(), s.length());
	}

	void push_boolean(bool b)
	{
		lua_pushboolean(L, b ? 1 : 0);
	}

#define TEMPORARY "LUAINTERP_INTERNAL_COMMAND_TEMPORARY"

	const char* eval_lua_lua = "loadstring(" TEMPORARY ")();";
	const char* run_lua_lua = "dofile(" TEMPORARY ");";

	void run_lua_fragment() throw(std::bad_alloc)
	{
		if(recursive_flag)
			return;
#if LUA_VERSION_NUM == 501
		int t = lua_load(L, read_lua_fragment, NULL, "run_lua_fragment");
#endif
#if LUA_VERSION_NUM == 502
		int t = lua_load(L, read_lua_fragment, NULL, "run_lua_fragment", "bt");
#endif
		if(t == LUA_ERRSYNTAX) {
			messages << "Can't run Lua: Internal syntax error: " << lua_tostring(L, -1) << std::endl;
			lua_pop(L, 1);
			return;
		}
		if(t == LUA_ERRMEM) {
			messages << "Can't run Lua: Out of memory" << std::endl;
			lua_pop(L, 1);
			return;
		}
		recursive_flag = true;
		int r = lua_pcall(L, 0, 0, 0);
		recursive_flag = false;
		if(r == LUA_ERRRUN) {
			messages << "Error running Lua hunk: " << lua_tostring(L, -1)  << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRMEM) {
			messages << "Error running Lua hunk: Out of memory" << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRERR) {
			messages << "Error running Lua hunk: Double Fault???" << std::endl;
			lua_pop(L, 1);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
	}

	void do_eval_lua(const std::string& c) throw(std::bad_alloc)
	{
		push_string(c);
		lua_setglobal(L, TEMPORARY);
		luareader_fragment = eval_lua_lua;
		run_lua_fragment();
	}

	void do_run_lua(const std::string& c) throw(std::bad_alloc)
	{
		push_string(c);
		lua_setglobal(L, TEMPORARY);
		luareader_fragment = run_lua_lua;
		run_lua_fragment();
	}

	void run_lua_cb(int args) throw()
	{
		recursive_flag = true;
		int r = lua_pcall(L, args, 0, 0);
		recursive_flag = false;
		if(r == LUA_ERRRUN) {
			messages << "Error running Lua callback: " << lua_tostring(L, -1)  << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRMEM) {
			messages << "Error running Lua callback: Out of memory" << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRERR) {
			messages << "Error running Lua callback: Double Fault???" << std::endl;
			lua_pop(L, 1);
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

	void copy_system_tables(lua_State* L)
	{
		lua_getglobal(L, "_G");
		lua_newtable(L);
		lua_pushnil(L);
		while(lua_next(L, -3)) {
			//Stack: _SYSTEM, KEY, VALUE
			lua_pushvalue(L, -2);
			lua_pushvalue(L, -2);
			//Stack: _SYSTEM, KEY, VALUE, KEY, VALUE
			lua_rawset(L, -5);
			//Stack: _SYSTEM, KEY, VALUE
			lua_pop(L, 1);
			//Stack: _SYSTEM, KEY
		}
		lua_newtable(L);
		lua_pushcfunction(L, system_write_error);
		lua_setfield(L, -2, "__newindex");
		lua_setmetatable(L, -2);
		lua_setglobal(L, "_SYSTEM");
	}

	void run_sysrc_lua(lua_State* L)
	{
		do_eval_lua(lua_sysrc_script);
	}

}

void lua_callback_do_paint(struct lua_render_context* ctx, bool non_synthetic) throw()
{
	if(!callback_exists("on_paint"))
		return;
	lua_render_ctx = ctx;
	push_boolean(non_synthetic);
	run_lua_cb(1);
	lua_render_ctx = NULL;
}

void lua_callback_do_video(struct lua_render_context* ctx) throw()
{
	if(!callback_exists("on_video"))
		return;
	lua_render_ctx = ctx;
	run_lua_cb(0);
	lua_render_ctx = NULL;
}

void lua_callback_do_reset() throw()
{
	if(!callback_exists("on_reset"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_frame() throw()
{
	if(!callback_exists("on_frame"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_rewind() throw()
{
	if(!callback_exists("on_rewind"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_idle() throw()
{
	lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
	if(!callback_exists("on_idle"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_timer() throw()
{
	lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;	
	if(!callback_exists("on_timer"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_frame_emulated() throw()
{
	if(!callback_exists("on_frame_emulated"))
		return;
	run_lua_cb(0);
}

void lua_callback_do_readwrite() throw()
{
	if(!callback_exists("on_readwrite"))
		return;
	run_lua_cb(0);
}

void lua_callback_startup() throw()
{
	lua_booted_flag = true;
	if(!callback_exists("on_startup"))
		return;
	run_lua_cb(0);
}

void lua_callback_pre_load(const std::string& name) throw()
{
	if(!callback_exists("on_pre_load"))
		return;
	push_string(name);
	run_lua_cb(1);
}

void lua_callback_err_load(const std::string& name) throw()
{
	if(!callback_exists("on_err_load"))
		return;
	push_string(name);
	run_lua_cb(1);
}

void lua_callback_post_load(const std::string& name, bool was_state) throw()
{
	if(!callback_exists("on_post_load"))
		return;
	push_string(name);
	push_boolean(was_state);
	run_lua_cb(2);
}

void lua_callback_pre_save(const std::string& name, bool is_state) throw()
{
	if(!callback_exists("on_pre_save"))
		return;
	push_string(name);
	push_boolean(is_state);
	run_lua_cb(2);
}

void lua_callback_err_save(const std::string& name) throw()
{
	if(!callback_exists("on_err_save"))
		return;
	push_string(name);
	run_lua_cb(1);
}

void lua_callback_post_save(const std::string& name, bool is_state) throw()
{
	if(!callback_exists("on_post_save"))
		return;
	push_string(name);
	push_boolean(is_state);
	run_lua_cb(2);
}

void lua_callback_do_input(controller_frame& data, bool subframe) throw()
{
	if(!callback_exists("on_input"))
		return;
	lua_input_controllerdata = &data;
	push_boolean(subframe);
	run_lua_cb(1);
	lua_input_controllerdata = NULL;
}

void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw()
{
	if(!callback_exists("on_snoop"))
		return;
	lua_pushnumber(L, port);
	lua_pushnumber(L, controller);
	lua_pushnumber(L, index);
	lua_pushnumber(L, value);
	run_lua_cb(4);
}

namespace
{
	function_ptr_command<const std::string&> evaluate_lua(lsnes_cmd, "evaluate-lua", "Evaluate expression in "
		"Lua VM", "Syntax: evaluate-lua <expression>\nEvaluates <expression> in Lua VM.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			do_eval_lua(args);
		});

	function_ptr_command<arg_filename> run_lua(lsnes_cmd, "run-lua", "Run Lua script in Lua VM",
		"Syntax: run-lua <file>\nRuns <file> in Lua VM.\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error)
		{
			do_run_lua(args);
		});

	function_ptr_command<> reset_lua(lsnes_cmd, "reset-lua", "Reset the Lua VM",
		"Syntax: reset-lua\nReset the Lua VM.\n",
		[]() throw(std::bad_alloc, std::runtime_error)
		{
			lua_State* L = lua_initialized;
			lua_initialized = NULL;
			init_lua(true);
			if(!lua_initialized) {
				lua_initialized = L;
				return;
			}
			lua_close(L);
			messages << "Lua VM reset" << std::endl;
		});

}

void lua_callback_quit() throw()
{
	if(!callback_exists("on_quit"))
		return;
	run_lua_cb(0);
}

void lua_callback_keyhook(const std::string& key, const struct keygroup::parameters& p) throw()
{
	if(!callback_exists("on_keyhook"))
		return;
	lua_pushstring(L, key.c_str());
	push_keygroup_parameters(L, p);
	run_lua_cb(2);
}

void init_lua(bool soft) throw()
{
	L = lua_newstate(alloc, NULL);
	if(!L) {
		messages << "Can't initialize Lua." << std::endl;
		if(soft)
			return;
		fatal_error();
	}
	luaL_openlibs(L);

	register_lua_functions(L);
	run_sysrc_lua(L);
	copy_system_tables(L);
}

void quit_lua() throw()
{
	if(lua_initialized) {
		lua_close(lua_initialized);
		lua_initialized = NULL;
	}
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
				run_lua_cb(0);
			mainloop_restore_state(u2->state, u2->secs, u2->ssecs);
			mov.fast_load(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			if(callback_exists("on_post_rewind"))
				run_lua_cb(0);
		} catch(...) {
			return;
		}
	} else {
		//Save
		if(callback_exists("on_set_rewind")) {
			lua_unsaferewind* u2 = lua_class<lua_unsaferewind>::create(L);
			u2->state = save;
			u2->secs = secs,
			u2->ssecs = ssecs;
			mov.fast_save(u2->frame, u2->ptr, u2->lag, u2->pollcounters);
			run_lua_cb(1);
		}
		
	}
}


bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
bool lua_supported = true;

DECLARE_LUACLASS(lua_unsaferewind, "UNSAFEREWIND");
