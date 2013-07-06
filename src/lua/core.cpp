#include "core/command.hpp"
#include "lua/internal.hpp"
#include "core/framerate.hpp"
#include "core/moviefile.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"

namespace
{
	function_ptr_luafun lua_print(LS, "print2", [](lua_state& L, const std::string& fname) -> int {
		int stacksize = 0;
		while(!L.isnone(stacksize + 1))
		stacksize++;
		std::string toprint;
		bool first = true;
		for(int i = 0; i < stacksize; i++) {
			size_t len;
			const char* tmp = NULL;
			if(L.isnil(i + 1)) {
				tmp = "nil";
				len = 3;
			} else if(L.isboolean(i + 1) && L.toboolean(i + 1)) {
				tmp = "true";
				len = 4;
			} else if(L.isboolean(i + 1) && !L.toboolean(i + 1)) {
				tmp = "false";
				len = 5;
			} else {
				tmp = L.tolstring(i + 1, &len);
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
		platform::message(toprint);
		return 0;
	});

	function_ptr_luafun lua_exec(LS, "exec", [](lua_state& L, const std::string& fname) -> int {
		std::string text = L.get_string(1, fname.c_str());
		lsnes_cmd.invoke(text);
		return 0;
	});

	function_ptr_luafun lua_booted(LS, "emulator_ready", [](lua_state& L, const std::string& fname) -> int {
		L.pushboolean(lua_booted_flag ? 1 : 0);
		return 1;
	});

	function_ptr_luafun lua_utime(LS, "utime", [](lua_state& L, const std::string& fname) -> int {
		uint64_t t = get_utime();
		L.pushnumber(t / 1000000);
		L.pushnumber(t % 1000000);
		return 2;
	});

	function_ptr_luafun lua_idle_time(LS, "set_idle_timeout", [](lua_state& L, const std::string& fname) -> int {
		lua_idle_hook_time = get_utime() + L.get_numeric_argument<uint64_t>(1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_timer_time(LS, "set_timer_timeout", [](lua_state& L, const std::string& fname) ->
		int {
		lua_timer_hook_time = get_utime() + L.get_numeric_argument<uint64_t>(1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_busaddr(LS, "bus_address", [](lua_state& L, const std::string& fname) -> int {
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		auto busrange = our_rom->rtype->get_bus_map();
		if(!busrange.second) {
			L.pushstring("This platform does not have bus mapping");
			L.error();
			return 0;
		}
		L.pushnumber(busrange.first + (addr % busrange.second));
		return 1;
	});
}
