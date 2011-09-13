#include "lua.hpp"
#include "misc.hpp"
#include "memorymanip.hpp"
#include "mainloop.hpp"
extern "C" {
#include <lua.h>
#include <lualib.h>
}
#include <iostream>
#include "fieldsplit.hpp"
#define BITWISE_BITS 48
#define BITWISE_MASK ((1ULL << (BITWISE_BITS)) - 1)

#define SETFIELDFUN(LSS, idx, name, fun) do { lua_pushcfunction(LSS, fun); lua_setfield(LSS, (idx) - 1, name); \
	} while(0)

namespace 
{
	window* tmp_win;
	lua_State* L;
	lua_render_context* rctx = NULL;
	bool recursive_flag = false;
	commandhandler* cmdhnd;
	const char* luareader_fragment = NULL;
	controls_t* controllerdata = NULL;

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
		if(lua_requests_repaint && cmdhnd) {
			std::string c = "repaint";
			lua_requests_repaint = false;
			cmdhnd->docommand(c, win);
		}
	}

	template<typename T>
	T get_numeric_argument(lua_State* LS, unsigned argindex, const char* fname)
	{
		if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
			lua_pushfstring(L, "argument #%i to %s must be numeric", argindex, fname);
			lua_error(LS);
		}
		return static_cast<T>(lua_tonumber(LS, argindex));
	}

	std::string get_string_argument(lua_State* LS, unsigned argindex, const char* fname)
	{
		if(lua_isnone(LS, argindex)) {
			lua_pushfstring(L, "argument #%i to %s must be string", argindex, fname);
			lua_error(LS);
		}
		size_t len;
		const char* f = lua_tolstring(LS, argindex, &len);
		if(!f) {
			lua_pushfstring(L, "argument #%i to %s must be string", argindex, fname);
			lua_error(LS);
		}
		return std::string(f, f + len);
	}

	bool get_boolean_argument(lua_State* LS, unsigned argindex, const char* fname)
	{
		if(lua_isnone(LS, argindex) || !lua_isboolean(LS, argindex)) {
			lua_pushfstring(L, "argument #%i to %s must be boolean", argindex, fname);
			lua_error(LS);
		}
		return (lua_toboolean(LS, argindex) != 0);
	}

	template<typename T>
	void get_numeric_argument(lua_State* LS, unsigned argindex, T& value, const char* fname)
	{
		if(lua_isnoneornil(LS, argindex))
			return;
		if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
			lua_pushfstring(L, "argument #%i to %s must be numeric if present", argindex, fname);
			lua_error(LS);
		}
		value = static_cast<T>(lua_tonumber(LS, argindex));
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
		if(lua_requests_repaint && cmdhnd) {
			std::string c = "repaint";
			lua_requests_repaint = false;
			cmdhnd->docommand(c, win);
		}
	}

	int lua_symmetric_bitwise(lua_State* LS, uint64_t (*combine)(uint64_t chain, uint64_t arg), uint64_t init)
	{
		int stacksize = 0;
		while(!lua_isnone(LS, stacksize + 1))
			stacksize++;
		uint64_t ret = init;
		for(int i = 0; i < stacksize; i++)
			ret = combine(ret, get_numeric_argument<uint64_t>(LS, i + 1, "<bitwise function>"));
		lua_pushnumber(LS, ret);
		return 1;
	}

	int lua_shifter(lua_State* LS, uint64_t (*shift)(uint64_t base, uint64_t amount, uint64_t bits))
	{
		uint64_t base;
		uint64_t amount = 1;
		uint64_t bits = BITWISE_BITS;
		base = get_numeric_argument<uint64_t>(LS, 1, "<shift function>");
		get_numeric_argument(LS, 2, amount, "<shift function>");
		get_numeric_argument(LS, 3, bits, "<shift function>");
		lua_pushnumber(LS, shift(base, amount, bits));
		return 1;
	}

	uint64_t combine_none(uint64_t chain, uint64_t arg)
	{
		return (chain & ~arg) & BITWISE_MASK;
	}

	uint64_t combine_any(uint64_t chain, uint64_t arg)
	{
		return (chain | arg) & BITWISE_MASK;
	}

	uint64_t combine_all(uint64_t chain, uint64_t arg)
	{
		return (chain & arg) & BITWISE_MASK;
	}

	uint64_t combine_parity(uint64_t chain, uint64_t arg)
	{
		return (chain ^ arg) & BITWISE_MASK;
	}

	uint64_t shift_lrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base = (base << amount) | (base >> (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_rrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base = (base >> amount) | (base << (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_lshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base <<= amount;
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_lrshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base >>= amount;
		return base & BITWISE_MASK;
	}

	uint64_t shift_arshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		bool negative = ((base >> (bits - 1)) != 0);
		base >>= amount;
		base |= ((negative ? BITWISE_MASK : 0) << (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	int lua_bit_none(lua_State* LS)
	{
		return lua_symmetric_bitwise(LS, combine_none, BITWISE_MASK);
	}

	int lua_bit_any(lua_State* LS)
	{
		return lua_symmetric_bitwise(LS, combine_any, 0);
	}

	int lua_bit_all(lua_State* LS)
	{
		return lua_symmetric_bitwise(LS, combine_all, BITWISE_MASK);
	}

	int lua_bit_parity(lua_State* LS)
	{
		return lua_symmetric_bitwise(LS, combine_parity, 0);
	}

	int lua_bit_lrotate(lua_State* LS)
	{
		return lua_shifter(LS, shift_lrotate);
	}

	int lua_bit_rrotate(lua_State* LS)
	{
		return lua_shifter(LS, shift_rrotate);
	}

	int lua_bit_lshift(lua_State* LS)
	{
		return lua_shifter(LS, shift_lshift);
	}

	int lua_bit_arshift(lua_State* LS)
	{
		return lua_shifter(LS, shift_arshift);
	}

	int lua_bit_lrshift(lua_State* LS)
	{
		return lua_shifter(LS, shift_lrshift);
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
	}

	int lua_gui_resolution(lua_State* LS)
	{
		if(!rctx)
			return 0;
		lua_pushnumber(LS, rctx->width);
		lua_pushnumber(LS, rctx->height);
		lua_pushnumber(LS, rctx->rshift);
		lua_pushnumber(LS, rctx->gshift);
		lua_pushnumber(LS, rctx->bshift);
		return 5;
	}

	int lua_gui_set_gap(lua_State* LS, uint32_t lua_render_context::*gap)
	{
		if(!rctx)
			return 0;
		uint32_t g = get_numeric_argument<uint32_t>(LS, 1, "gui.<direction>_gap");
		if(g > 8192)
			return 0;	//Ignore ridiculous gap.
		rctx->*gap = g;
	}

	int lua_gui_set_left_gap(lua_State* LS)
	{
		lua_gui_set_gap(LS, &lua_render_context::left_gap);
	}

	int lua_gui_set_right_gap(lua_State* LS)
	{
		lua_gui_set_gap(LS, &lua_render_context::right_gap);
	}

	int lua_gui_set_top_gap(lua_State* LS)
	{
		lua_gui_set_gap(LS, &lua_render_context::top_gap);
	}

	int lua_gui_set_bottom_gap(lua_State* LS)
	{
		lua_gui_set_gap(LS, &lua_render_context::bottom_gap);
	}

	int lua_gui_text(lua_State* LS)
	{
		if(!rctx)
			return 0;
		uint32_t x255 = 255;
		uint32_t fgc = (x255 << rctx->rshift) | (x255 << rctx->gshift) | (x255 << rctx->bshift);
		uint32_t bgc = 0;
		uint16_t fga = 256;
		uint16_t bga = 0;
		int32_t _x = get_numeric_argument<int32_t>(LS, 1, "gui.text");
		int32_t _y = get_numeric_argument<int32_t>(LS, 2, "gui.text");
		get_numeric_argument<uint32_t>(LS, 4, fgc, "gui.text");
		get_numeric_argument<uint16_t>(LS, 5, fga, "gui.text");
		get_numeric_argument<uint32_t>(LS, 6, bgc, "gui.text");
		get_numeric_argument<uint16_t>(LS, 7, bga, "gui.text");
		std::string text = get_string_argument(LS, 3, "gui.text");
		rctx->queue->add(*new render_object_text(_x, _y, text, fgc, fga, bgc, bga));
		return 0;
	}

	int lua_gui_request_repaint(lua_State* LS)
	{
		lua_requests_repaint = true;
		return 0;
	}

	int lua_gui_update_subframe(lua_State* LS)
	{
		lua_requests_subframe_paint = get_boolean_argument(LS, 1, "gui.subframe_update");
		return 0;
	}

	int lua_exec(lua_State* LS)
	{
		std::string text = get_string_argument(LS, 1, "exec");
		cmdhnd->docommand(text, tmp_win);
		return 0;
	}

	template<typename T, typename U>
	int lua_read_memory(lua_State* LS, U (*rfun)(uint32_t addr))
	{
		uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, "memory.read<type>");
		lua_pushnumber(LS, static_cast<T>(rfun(addr)));
		return 1;
	}

	template<typename T>
	int lua_write_memory(lua_State* LS, bool (*wfun)(uint32_t addr, T value))
	{
		uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, "memory.write<type>");
		T value = get_numeric_argument<T>(LS, 2, "memory.write<type>");
		wfun(addr, value);
		return 0;
	}

	int lua_memory_readbyte(lua_State* LS)
	{
		return lua_read_memory<uint8_t, uint8_t>(LS, memory_read_byte);
	}

	int lua_memory_readsbyte(lua_State* LS)
	{
		return lua_read_memory<int8_t, uint8_t>(LS, memory_read_byte);
	}

	int lua_memory_readword(lua_State* LS)
	{
		return lua_read_memory<uint16_t, uint16_t>(LS, memory_read_word);
	}

	int lua_memory_readsword(lua_State* LS)
	{
		return lua_read_memory<int16_t, uint16_t>(LS, memory_read_word);
	}

	int lua_memory_readdword(lua_State* LS)
	{
		return lua_read_memory<uint32_t, uint32_t>(LS, memory_read_dword);
	}

	int lua_memory_readsdword(lua_State* LS)
	{
		return lua_read_memory<int32_t, uint32_t>(LS, memory_read_dword);
	}

	int lua_memory_readqword(lua_State* LS)
	{
		return lua_read_memory<uint64_t, uint64_t>(LS, memory_read_qword);
	}

	int lua_memory_readsqword(lua_State* LS)
	{
		return lua_read_memory<int64_t, uint64_t>(LS, memory_read_qword);
	}

	int lua_memory_writebyte(lua_State* LS)
	{
		return lua_write_memory(LS, memory_write_byte);
	}

	int lua_memory_writeword(lua_State* LS)
	{
		return lua_write_memory(LS, memory_write_word);
	}

	int lua_memory_writedword(lua_State* LS)
	{
		return lua_write_memory(LS, memory_write_dword);
	}

	int lua_memory_writeqword(lua_State* LS)
	{
		return lua_write_memory(LS, memory_write_qword);
	}

	int lua_input_set(lua_State* LS)
	{
		if(!controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, "input.set");
		unsigned index = get_numeric_argument<unsigned>(LS, 2, "input.set");
		short value = get_numeric_argument<short>(LS, 3, "input.set");
		if(controller > 7 || index > 11)
			return 0;
		(*controllerdata)(controller >> 2, controller & 3, index) = value;
		return 0;
	}

	int lua_input_get(lua_State* LS)
	{
		if(!controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, "input.set");
		unsigned index = get_numeric_argument<unsigned>(LS, 2, "input.set");
		if(controller > 7 || index > 11)
			return 0;
		lua_pushnumber(LS, (*controllerdata)(controller >> 2, controller & 3, index));
		return 1;
	}

	int lua_input_reset(lua_State* LS)
	{
		if(!controllerdata)
			return 0;
		long cycles = 0;
		get_numeric_argument(LS, 1, cycles, "input.reset");
		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		(*controllerdata)(CONTROL_SYSTEM_RESET) = 1;
		(*controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_HI) = hi;
		(*controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_LO) = lo;
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
	rctx = ctx;
	run_lua_cb(0, win);
	rctx = NULL;
}

void lua_callback_do_video(struct lua_render_context* ctx, window* win) throw()
{
	if(!callback_exists("on_video"))
		return;
	rctx = ctx;
	run_lua_cb(0, win);
	rctx = NULL;
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
	controllerdata = &data;
	push_boolean(subframe);
	run_lua_cb(1, win);
	controllerdata = NULL;
}


bool lua_command(const std::string& cmd, window* win) throw(std::bad_alloc)
{
	if(is_cmd_prefix(cmd, "eval-lua")) {
		tokensplitter t(cmd);
		std::string dummy = t;
		do_eval_lua(t.tail(), win);
		return true;
	}
	if(is_cmd_prefix(cmd, "run-lua")) {
		tokensplitter t(cmd);
		std::string dummy = t;
		do_run_lua(t.tail(), win);
		return true;
	}
	return false;
}

void lua_callback_quit(window* win) throw()
{
	if(!callback_exists("on_quit"))
		return;
	run_lua_cb(0, win);
}

void lua_set_commandhandler(commandhandler& cmdh) throw()
{
	cmdhnd = &cmdh;
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

	//Bit table.
	lua_newtable(L);
	SETFIELDFUN(L, -1, "none", lua_bit_none);
	SETFIELDFUN(L, -1, "bnot", lua_bit_none);
	SETFIELDFUN(L, -1, "any", lua_bit_any);
	SETFIELDFUN(L, -1, "bor", lua_bit_any);
	SETFIELDFUN(L, -1, "all", lua_bit_all);
	SETFIELDFUN(L, -1, "band", lua_bit_all);
	SETFIELDFUN(L, -1, "parity", lua_bit_parity);
	SETFIELDFUN(L, -1, "bxor", lua_bit_parity);
	SETFIELDFUN(L, -1, "lrotate", lua_bit_lrotate);
	SETFIELDFUN(L, -1, "rrotate", lua_bit_rrotate);
	SETFIELDFUN(L, -1, "lshift", lua_bit_lshift);
	SETFIELDFUN(L, -1, "arshift", lua_bit_arshift);
	SETFIELDFUN(L, -1, "lrshift", lua_bit_lrshift);
	lua_setglobal(L, "bit");

	//Gui table.
	lua_newtable(L);
	SETFIELDFUN(L, -1, "resolution", lua_gui_resolution);
	SETFIELDFUN(L, -1, "left_gap", lua_gui_set_left_gap);
	SETFIELDFUN(L, -1, "right_gap", lua_gui_set_right_gap);
	SETFIELDFUN(L, -1, "top_gap", lua_gui_set_top_gap);
	SETFIELDFUN(L, -1, "bottom_gap", lua_gui_set_bottom_gap);
	SETFIELDFUN(L, -1, "text", lua_gui_text);
	SETFIELDFUN(L, -1, "repaint", lua_gui_request_repaint);
	SETFIELDFUN(L, -1, "subframe_update", lua_gui_update_subframe);
	lua_setglobal(L, "gui");

	//Memory table.
	lua_newtable(L);
	SETFIELDFUN(L, -1, "readbyte", lua_memory_readbyte);
	SETFIELDFUN(L, -1, "readsbyte", lua_memory_readsbyte);
	SETFIELDFUN(L, -1, "writebyte", lua_memory_writebyte);
	SETFIELDFUN(L, -1, "readword", lua_memory_readword);
	SETFIELDFUN(L, -1, "readsword", lua_memory_readsword);
	SETFIELDFUN(L, -1, "writeword", lua_memory_writeword);
	SETFIELDFUN(L, -1, "readdword", lua_memory_readdword);
	SETFIELDFUN(L, -1, "readsdword", lua_memory_readsdword);
	SETFIELDFUN(L, -1, "writedword", lua_memory_writedword);
	SETFIELDFUN(L, -1, "readqword", lua_memory_readqword);
	SETFIELDFUN(L, -1, "readsqword", lua_memory_readsqword);
	SETFIELDFUN(L, -1, "writeqword", lua_memory_writeqword);
	lua_setglobal(L, "memory");

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

	//TODO: Add some functions into the Lua state.
}

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
