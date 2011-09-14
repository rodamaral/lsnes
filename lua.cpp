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

#define SETFIELDFUN(LSS, idx, name, fun) do { lua_pushcfunction(LSS, fun); lua_setfield(LSS, (idx) - 1, name); \
	} while(0)

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

	int lua_print(lua_State* LS)
	{
		int stacksize = 0;
		while(!lua_isnone(LS, stacksize + 1))
			stacksize++;
		std::string toprint;
		bool first = true;
		for(int i = 0; i < stacksize; i++) {
			size_t len;
			const char* tmp = NULL;
			if(lua_isnil(LS, i + 1)) {
				tmp = "nil";
				len = 3;
			} else if(lua_isboolean(LS, i + 1) && lua_toboolean(LS, i + 1)) {
				tmp = "true";
				len = 4;
			} else if(lua_isboolean(LS, i + 1) && !lua_toboolean(LS, i + 1)) {
				tmp = "false";
				len = 5;
			} else {
				tmp = lua_tolstring(LS, i + 1, &len);
				if(!tmp) {
					tmp = "(unprintable)";
					len = 13;
				}
			}
			std::string localmsg(tmp, tmp + len);
			if(first)
				toprint = localmsg;
			else
				toprint = toprint + "\t" + localmsg;
			first = false;
		}
		tmp_win->message(toprint);
		return 0;
	}


	int lua_exec(lua_State* LS)
	{
		std::string text = get_string_argument(LS, 1, "exec");
		command::invokeC(text, tmp_win);
		return 0;
	}

	int lua_input_set(lua_State* LS)
	{
		if(!lua_input_controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, "input.set");
		unsigned index = get_numeric_argument<unsigned>(LS, 2, "input.set");
		short value = get_numeric_argument<short>(LS, 3, "input.set");
		if(controller > 7 || index > 11)
			return 0;
		(*lua_input_controllerdata)(controller >> 2, controller & 3, index) = value;
		return 0;
	}

	int lua_input_get(lua_State* LS)
	{
		if(!lua_input_controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, "input.set");
		unsigned index = get_numeric_argument<unsigned>(LS, 2, "input.set");
		if(controller > 7 || index > 11)
			return 0;
		lua_pushnumber(LS, (*lua_input_controllerdata)(controller >> 2, controller & 3, index));
		return 1;
	}

	int lua_input_reset(lua_State* LS)
	{
		if(!lua_input_controllerdata)
			return 0;
		long cycles = 0;
		get_numeric_argument(LS, 1, cycles, "input.reset");
		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET) = 1;
		(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_HI) = hi;
		(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_LO) = lo;
		return 0;
	}

	int lua_hostmemory_read(lua_State* LS)
	{
		size_t address = get_numeric_argument<size_t>(LS, 1, "hostmemory.read");
		auto& h = get_host_memory();
		if(address >= h.size()) {
			lua_pushboolean(LS, 0);
			return 1;
		}
		lua_pushnumber(LS, static_cast<uint8_t>(h[address]));
		return 1;
	}

	int lua_hostmemory_write(lua_State* LS)
	{
		size_t address = get_numeric_argument<size_t>(LS, 1, "hostmemory.write");
		uint8_t value = get_numeric_argument<uint8_t>(LS, 2, "hostmemory.write");
		auto& h = get_host_memory();
		if(address >= h.size())
			h.resize(address + 1);
		h[address] = value;
		return 0;
	}

	int lua_movie_currentframe(lua_State* LS)
	{
		auto& m = get_movie();
		lua_pushnumber(LS, m.get_current_frame());
		return 1;
	}

	int lua_movie_framecount(lua_State* LS)
	{
		auto& m = get_movie();
		lua_pushnumber(LS, m.get_frame_count());
		return 1;
	}

	int lua_movie_readonly(lua_State* LS)
	{
		auto& m = get_movie();
		lua_pushboolean(LS, m.readonly_mode() ? 1 : 0);
		return 1;
	}

	int lua_movie_set_readwrite(lua_State* LS)
	{
		auto& m = get_movie();
		m.readonly_mode(false);
		return 0;
	}

	int lua_movie_frame_subframes(lua_State* LS)
	{
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
		auto& m = get_movie();
		lua_pushnumber(LS, m.frame_subframes(frame));
		return 1;
	}

	int lua_movie_read_subframe(lua_State* LS)
	{
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
		uint64_t subframe = get_numeric_argument<uint64_t>(LS, 2, "movie.frame_subframes");
		auto& m = get_movie();
		controls_t r = m.read_subframe(frame, subframe);
		lua_newtable(LS);
		for(size_t i = 0; i < TOTAL_CONTROLS; i++) {
			lua_pushnumber(LS, i);
			lua_pushnumber(LS, r(i));
			lua_settable(L, -3);
		}
		return 1;
		
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

	//Some globals
	lua_pushcfunction(L, lua_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, lua_exec);
	lua_setglobal(L, "exec");

	//Input table
	lua_newtable(L);
	SETFIELDFUN(L, -1, "get", lua_input_get);
	SETFIELDFUN(L, -1, "set", lua_input_set);
	SETFIELDFUN(L, -1, "reset", lua_input_reset);
	lua_setglobal(L, "input");

	//Hostmemory table.
	lua_newtable(L);
	SETFIELDFUN(L, -1, "read", lua_hostmemory_read);
	SETFIELDFUN(L, -1, "write", lua_hostmemory_write);
	lua_setglobal(L, "hostmemory");

	//Movie table.
	lua_newtable(L);
	SETFIELDFUN(L, -1, "currentframe", lua_movie_currentframe);
	SETFIELDFUN(L, -1, "frame_subframes", lua_movie_frame_subframes);
	SETFIELDFUN(L, -1, "framecount", lua_movie_framecount);
	SETFIELDFUN(L, -1, "read_subframe", lua_movie_read_subframe);
	SETFIELDFUN(L, -1, "readonly", lua_movie_readonly);
	SETFIELDFUN(L, -1, "set_readwrite", lua_movie_set_readwrite);
	lua_setglobal(L, "movie");

	register_lua_functions(L);
}

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
