#include "core/command.hpp"
#include "lua/internal.hpp"
#include "core/framerate.hpp"
#include "core/moviefile.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include <sstream>

namespace
{
	std::string luavalue_to_string(lua_state& L, int index, std::set<const void*>& printed, bool quote)
	{
		switch(L.type(index)) {
		case LUA_TNONE:
			return "none";
		case LUA_TNIL:
			return "nil";
		case LUA_TBOOLEAN:
			return L.toboolean(index) ? "true" : "false";
		case LUA_TNUMBER:
			return (stringfmt() << L.tonumber(index)).str();
		case LUA_TSTRING: {
			const char* tmp2;
			size_t len;
			tmp2 = L.tolstring(index, &len);
			if(quote)
				return "\"" + std::string(tmp2, tmp2 + len) + "\"";
			else
				return std::string(tmp2, tmp2 + len);
		}
		case LUA_TLIGHTUSERDATA:
			return (stringfmt() << "Lightuserdata:" << L.touserdata(index)).str();
		case LUA_TFUNCTION:
			return (stringfmt() << "Function:" << L.topointer(index)).str();
		case LUA_TTHREAD:
			return (stringfmt() << "Thread:" << L.topointer(index)).str();
			break;
		case LUA_TUSERDATA:
			return (stringfmt() << "Userdata<" << try_recognize_userdata(L, index) << "@"
				<< L.touserdata(index) << ">:[" << try_print_userdata(L, index) << "]").str();
		case LUA_TTABLE: {
			//Fun with recursion.
			const void* ptr = L.topointer(index);
			if(printed.count(ptr))
				return (stringfmt() << "<table:" << ptr << ">").str();
			printed.insert(ptr);
			std::ostringstream s;
			s << "<" << ptr << ">{";
			L.pushnil();
			bool first = true;
			while(L.next(index)) {
				if(!first)
					s << ", ";
				int stacktop = L.gettop();
				s << "[" << luavalue_to_string(L, stacktop - 1, printed, true) << "]="
					<< luavalue_to_string(L, stacktop, printed, true);
				first = false;
				L.pop(1);
			}
			s << "}";
			return s.str();
		}
		default:
			return (stringfmt() << "???:" << L.topointer(index)).str();
		}
	}

	function_ptr_luafun lua_tostringx(lua_func_misc, "tostringx", [](lua_state& L, const std::string& fname) ->
		int {
		std::set<const void*> tmp2;
		std::string y = luavalue_to_string(L, 1, tmp2, false);
		L.pushlstring(y.c_str(), y.length());
		return 1;
	});

	function_ptr_luafun lua_print(lua_func_misc, "print2", [](lua_state& L, const std::string& fname) -> int {
		int stacksize = 0;
		while(!L.isnone(stacksize + 1))
		stacksize++;
		std::string toprint;
		bool first = true;
		for(int i = 0; i < stacksize; i++) {
			std::set<const void*> tmp2;
			std::string tmp = luavalue_to_string(L, i + 1, tmp2, false);
			if(first)
				toprint = tmp;
			else
				toprint = toprint + "\t" + tmp;
			first = false;
		}
		platform::message(toprint);
		return 0;
	});

	function_ptr_luafun lua_exec(lua_func_misc, "exec", [](lua_state& L, const std::string& fname) -> int {
		std::string text = L.get_string(1, fname.c_str());
		lsnes_cmd.invoke(text);
		return 0;
	});

	function_ptr_luafun lua_booted(lua_func_misc, "emulator_ready", [](lua_state& L, const std::string& fname)
		-> int {
		L.pushboolean(lua_booted_flag ? 1 : 0);
		return 1;
	});

	function_ptr_luafun lua_utime(lua_func_misc, "utime", [](lua_state& L, const std::string& fname) -> int {
		uint64_t t = get_utime();
		L.pushnumber(t / 1000000);
		L.pushnumber(t % 1000000);
		return 2;
	});

	function_ptr_luafun lua_idle_time(lua_func_misc, "set_idle_timeout", [](lua_state& L,
		const std::string& fname) -> int {
		lua_idle_hook_time = get_utime() + L.get_numeric_argument<uint64_t>(1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_timer_time(lua_func_misc, "set_timer_timeout", [](lua_state& L,
		const std::string& fname) -> int {
		lua_timer_hook_time = get_utime() + L.get_numeric_argument<uint64_t>(1, fname.c_str());
		return 0;
	});

	function_ptr_luafun lua_busaddr(lua_func_misc, "bus_address", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		auto busrange = our_rom.rtype->get_bus_map();
		if(!busrange.second) {
			L.pushstring("This platform does not have bus mapping");
			L.error();
			return 0;
		}
		L.pushnumber(busrange.first + (addr % busrange.second));
		return 1;
	});

	function_ptr_luafun mgetlagflag(lua_func_misc, "memory.get_lag_flag", [](lua_state& L,
		const std::string& fname) -> int {
		L.pushboolean(!(our_rom.rtype && our_rom.rtype->get_pflag()));
		return 1;
	});

	function_ptr_luafun msetlagflag(lua_func_misc, "memory.set_lag_flag", [](lua_state& L,
		const std::string& fname) -> int {
		if(our_rom.rtype)
			our_rom.rtype->set_pflag(!L.get_bool(1, fname.c_str()));
		return 0;
	});
}
