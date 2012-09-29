#include "core/command.hpp"
#include "core/emucore.hpp"
#include "lua/internal.hpp"
#include "core/framerate.hpp"
#include "core/window.hpp"

namespace
{
	function_ptr_luafun lua_print("print", [](lua_State* LS, const std::string& fname) -> int {
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
		platform::message(toprint);
		return 0;
	});

	function_ptr_luafun lua_exec("exec", [](lua_State* LS, const std::string& fname) -> int {
		std::string text = get_string_argument(LS, 1, fname.c_str());
		command::invokeC(text);
		return 0;
	});

	function_ptr_luafun lua_booted("emulator_ready", [](lua_State* LS, const std::string& fname) -> int {
		lua_pushboolean(LS, lua_booted_flag ? 1 : 0);
		return 1;
	});

	function_ptr_luafun lua_utime("utime", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t t = get_utime();
		lua_pushnumber(LS, t / 1000000);
		lua_pushnumber(LS, t % 1000000);
		return 2;
	});
	
	function_ptr_luafun lua_idle_time("set_idle_timeout", [](lua_State* LS, const std::string& fname) -> int {
		lua_idle_hook_time = get_utime() + get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_timer_time("set_timer_timeout", [](lua_State* LS, const std::string& fname) -> int {
		lua_timer_hook_time = get_utime() + get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_busaddr("bus_address", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		auto busrange = core_get_bus_map();
		if(!busrange.second) {
			lua_pushstring(LS, "This platform does not have bus mapping");
			lua_error(LS);
			return 0;
		}
		lua_pushnumber(LS, busrange.first + (addr % busrange.second));
		return 1;
	});
}
