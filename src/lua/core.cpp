#include "core/command.hpp"
#include "core/emucore.hpp"
#include "lua/internal.hpp"
#include "core/framerate.hpp"
#include "core/window.hpp"
#include "library/string.hpp"
#include <sstream>

namespace
{
	std::string luavalue_to_string(lua_State* LS, int index, std::set<const void*>& printed, bool quote)
	{
		switch(lua_type(LS, index)) {
		case LUA_TNONE:
			return "none";
		case LUA_TNIL:
			return "nil";
		case LUA_TBOOLEAN:
			return lua_toboolean(LS, index) ? "true" : "false";
		case LUA_TNUMBER:
			return (stringfmt() << lua_tonumber(LS, index)).str();
		case LUA_TSTRING: {
			const char* tmp2;
			size_t len;
			tmp2 = lua_tolstring(LS, index, &len);
			if(quote)
				return "\"" + std::string(tmp2, tmp2 + len) + "\"";
			else
				return std::string(tmp2, tmp2 + len);
		}
		case LUA_TLIGHTUSERDATA:
			return (stringfmt() << "Lightuserdata:" << lua_touserdata(LS, index)).str();
		case LUA_TFUNCTION:
			return (stringfmt() << "Function:" << lua_topointer(LS, index)).str();
		case LUA_TTHREAD:
			return (stringfmt() << "Thread:" << lua_topointer(LS, index)).str();
			break;
		case LUA_TUSERDATA:
			return (stringfmt() << "Userdata<" << try_recognize_userdata(LS, index) << ">:"
				<< lua_touserdata(LS, index)).str();
		case LUA_TTABLE: {
			//Fun with recursion.
			const void* ptr = lua_topointer(LS, index);
			if(printed.count(ptr))
				return (stringfmt() << "<table:" << ptr << ">").str();
			printed.insert(ptr);
			std::ostringstream s;
			s << "<" << ptr << ">{";
			lua_pushnil(LS);
			bool first = true;
			while(lua_next(LS, index)) {
				if(!first)
					s << ", ";
				int stacktop = lua_gettop(LS);
				s << "[" << luavalue_to_string(LS, stacktop - 1, printed, true) << "]="
					<< luavalue_to_string(LS, stacktop, printed, true);
				first = false;
				lua_pop(LS, 1);
			}
			s << "}";
			return s.str();
		}
		default:
			return (stringfmt() << "???:" << lua_topointer(LS, index)).str();
		}
	}

	function_ptr_luafun lua_tostringx("tostringx", [](lua_State* LS, const std::string& fname) -> int {
		std::set<const void*> tmp2;
		std::string y = luavalue_to_string(LS, 1, tmp2, false);
		lua_pushlstring(LS, y.c_str(), y.length());
		return 1;
	});

	function_ptr_luafun lua_print("print", [](lua_State* LS, const std::string& fname) -> int {
		int stacksize = 0;
		while(!lua_isnone(LS, stacksize + 1))
		stacksize++;
		std::string toprint;
		bool first = true;
		for(int i = 0; i < stacksize; i++) {
			std::set<const void*> tmp2;
			std::string tmp = luavalue_to_string(LS, i + 1, tmp2, false);
			if(first)
				toprint = tmp;
			else
				toprint = toprint + "\t" + tmp;
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
