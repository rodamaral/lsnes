#include "lua.hpp"
#include "lua-int.hpp"
#include "command.hpp"
#include "misc.hpp"
#include "memorymanip.hpp"
#include "mainloop.hpp"
#include <map>
#include <string>
extern "C" {
#include <lua.h>
#include <lualib.h>
}
#include <iostream>
#include "fieldsplit.hpp"

namespace
{
	std::map<std::string, lua_function*>* functions;
	lua_State* lua_initialized;
	window* tmp_win;

	int lua_trampoline_function(lua_State* L)
	{
		void* ptr = lua_touserdata(L, lua_upvalueindex(1));
		lua_function* f = reinterpret_cast<lua_function*>(ptr);
		return f->invoke(L, tmp_win);
	}

	//Pushes given table to top of stack, creating if needed.
	void recursive_lookup_table(lua_State* L, const std::string& tab)
	{
		if(tab == "") {
			lua_pushvalue(L, LUA_GLOBALSINDEX);
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
		void* ptr = reinterpret_cast<void*>((*functions)[fun]);
		lua_pushlightuserdata(L, ptr);
		lua_pushcclosure(L, lua_trampoline_function, 1);
		lua_setfield(L, -2, u2.c_str());
		lua_pop(L, 1);
	}

	void register_lua_functions(lua_State* L)
	{
		if(functions)
			for(auto i = functions->begin(); i != functions->end(); i++)
				register_lua_function(L, i->first);
		lua_initialized = L;
	}
}

lua_function::lua_function(const std::string& name) throw(std::bad_alloc)
{
	if(!functions)
		functions = new std::map<std::string, lua_function*>();
	(*functions)[fname = name] = this;
	if(lua_initialized)
		register_lua_function(lua_initialized, fname);
}

lua_function::~lua_function() throw()
{
	if(!functions)
		return;
	functions->erase(fname);
}

std::string get_string_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex)) {
		lua_pushfstring(LS, "argument #%i to %s must be string", argindex, fname);
		lua_error(LS);
	}
	size_t len;
	const char* f = lua_tolstring(LS, argindex, &len);
	if(!f) {
		lua_pushfstring(LS, "argument #%i to %s must be string", argindex, fname);
		lua_error(LS);
	}
	return std::string(f, f + len);
}

bool get_boolean_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex) || !lua_isboolean(LS, argindex)) {
		lua_pushfstring(LS, "argument #%i to %s must be boolean", argindex, fname);
		lua_error(LS);
	}
	return (lua_toboolean(LS, argindex) != 0);
}

lua_render_context* lua_render_ctx = NULL;
controls_t* lua_input_controllerdata = NULL;

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
		if(news)
			return realloc(old, news);
		else
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

	void run_lua_fragment(window* win) throw(std::bad_alloc)
	{
		if(recursive_flag)
			return;
		int t = lua_load(L, read_lua_fragment, NULL, "run_lua_fragment");
		if(t == LUA_ERRSYNTAX) {
			out(win) << "Can't run Lua: Internal syntax error: " << lua_tostring(L, -1) << std::endl;
			lua_pop(L, 1);
			return;
		}
		if(t == LUA_ERRMEM) {
			out(win) << "Can't run Lua: Out of memory" << std::endl;
			lua_pop(L, 1);
			return;
		}
		recursive_flag = true;
		tmp_win = win;
		int r = lua_pcall(L, 0, 0, 0);
		recursive_flag = false;
		if(r == LUA_ERRRUN) {
			out(win) << "Error running Lua hunk: " << lua_tostring(L, -1)  << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRMEM) {
			out(win) << "Error running Lua hunk: Out of memory" << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRERR) {
			out(win) << "Error running Lua hunk: Double Fault???" << std::endl;
			lua_pop(L, 1);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			command::invokeC("repaint", win);
		}
	}

	void do_eval_lua(const std::string& c, window* win) throw(std::bad_alloc)
	{
		push_string(c);
		lua_setglobal(L, TEMPORARY);
		luareader_fragment = eval_lua_lua;
		run_lua_fragment(win);
	}

	void do_run_lua(const std::string& c, window* win) throw(std::bad_alloc)
	{
		push_string(c);
		lua_setglobal(L, TEMPORARY);
		luareader_fragment = run_lua_lua;
		run_lua_fragment(win);
	}

	void run_lua_cb(int args, window* win) throw()
	{
		recursive_flag = true;
		tmp_win = win;
		int r = lua_pcall(L, args, 0, 0);
		recursive_flag = false;
		if(r == LUA_ERRRUN) {
			out(win) << "Error running Lua callback: " << lua_tostring(L, -1)  << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRMEM) {
			out(win) << "Error running Lua callback: Out of memory" << std::endl;
			lua_pop(L, 1);
		}
		if(r == LUA_ERRERR) {
			out(win) << "Error running Lua callback: Double Fault???" << std::endl;
			lua_pop(L, 1);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			command::invokeC("repaint", win);
		}
	}
}

void lua_callback_do_paint(struct lua_render_context* ctx, window* win) throw()
{
	if(!callback_exists("on_paint"))
		return;
	lua_render_ctx = ctx;
	run_lua_cb(0, win);
	lua_render_ctx = NULL;
}

void lua_callback_do_video(struct lua_render_context* ctx, window* win) throw()
{
	if(!callback_exists("on_video"))
		return;
	lua_render_ctx = ctx;
	run_lua_cb(0, win);
	lua_render_ctx = NULL;
}

void lua_callback_do_reset(window* win) throw()
{
	if(!callback_exists("on_reset"))
		return;
	run_lua_cb(0, win);
}

void lua_callback_do_readwrite(window* win) throw()
{
	if(!callback_exists("on_readwrite"))
		return;
	run_lua_cb(0, win);
}

void lua_callback_startup(window* win) throw()
{
	if(!callback_exists("on_startup"))
		return;
	run_lua_cb(0, win);
}

void lua_callback_pre_load(const std::string& name, window* win) throw()
{
	if(!callback_exists("on_pre_load"))
		return;
	push_string(name);
	run_lua_cb(1, win);
}

void lua_callback_err_load(const std::string& name, window* win) throw()
{
	if(!callback_exists("on_err_load"))
		return;
	push_string(name);
	run_lua_cb(1, win);
}

void lua_callback_post_load(const std::string& name, bool was_state, window* win) throw()
{
	if(!callback_exists("on_post_load"))
		return;
	push_string(name);
	push_boolean(was_state);
	run_lua_cb(2, win);
}

void lua_callback_pre_save(const std::string& name, bool is_state, window* win) throw()
{
	if(!callback_exists("on_pre_save"))
		return;
	push_string(name);
	push_boolean(is_state);
	run_lua_cb(2, win);
}

void lua_callback_err_save(const std::string& name, window* win) throw()
{
	if(!callback_exists("on_err_save"))
		return;
	push_string(name);
	run_lua_cb(1, win);
}

void lua_callback_post_save(const std::string& name, bool is_state, window* win) throw()
{
	if(!callback_exists("on_post_save"))
		return;
	push_string(name);
	push_boolean(is_state);
	run_lua_cb(2, win);
}

void lua_callback_do_input(controls_t& data, bool subframe, window* win) throw()
{
	if(!callback_exists("on_input"))
		return;
	lua_input_controllerdata = &data;
	push_boolean(subframe);
	run_lua_cb(1, win);
	lua_input_controllerdata = NULL;
}

void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value, window* win) throw()
{
	if(!callback_exists("on_snoop"))
		return;
	lua_pushnumber(L, port);
	lua_pushnumber(L, controller);
	lua_pushnumber(L, index);
	lua_pushnumber(L, value);
	run_lua_cb(4, win);
}

namespace
{
	class evallua : public command
	{
	public:
		evallua() throw(std::bad_alloc) : command("evaluate-lua") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Expected expression to evaluate");
			do_eval_lua(args, win);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Evaluate expression in Lua VM"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: evaluate-lua <expression>\n"
				"Evaluates <expression> in Lua VM.\n";
		}
	} evallua_o;

	class runlua : public command
	{
	public:
		runlua() throw(std::bad_alloc) : command("run-lua") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Expected script to run");
			do_run_lua(args, win);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Run Lua script in Lua VM"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: run-lua <file>\n"
				"Runs <file> in Lua VM.\n";
		}
	} runlua_o;
}

void lua_callback_quit(window* win) throw()
{
	if(!callback_exists("on_quit"))
		return;
	run_lua_cb(0, win);
}

void init_lua(window* win) throw()
{
	L = lua_newstate(alloc, NULL);
	if(!L) {
		out(win) << "Can't initialize Lua." << std::endl;
		fatal_error(win);
	}
	luaL_openlibs(L);

	register_lua_functions(L);
}

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
