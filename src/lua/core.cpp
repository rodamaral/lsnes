#include "core/command.hpp"
#include "lua/internal.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/moviefile.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include <sstream>

namespace
{
	std::string luavalue_to_string(lua::state& L, int index, std::set<const void*>& printed, bool quote)
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

	int identify_class(lua::state& L, lua::parameters& P)
	{
		if(!P.is_userdata())
			return 0;
		L.pushlstring(try_recognize_userdata(L, 1));
		return 1;
	}

	int tostringx(lua::state& L, lua::parameters& P)
	{
		std::set<const void*> tmp2;
		std::string y = luavalue_to_string(L, 1, tmp2, false);
		L.pushlstring(y);
		return 1;
	}

	int print2(lua::state& L, lua::parameters& P)
	{
		std::string toprint;
		bool first = true;
		while(P.more()) {
			int i = P.skip();
			std::set<const void*> tmp2;
			std::string tmp = luavalue_to_string(L, i, tmp2, false);
			if(first)
				toprint = tmp;
			else
				toprint = toprint + "\t" + tmp;
			first = false;
		}
		platform::message(toprint);
		return 0;
	}

	int exec(lua::state& L, lua::parameters& P)
	{
		std::string text;

		P(text);

		CORE().command->invoke(text);
		return 0;
	}

	int lookup_class(lua::state& L, lua::parameters& P)
	{
		std::string clazz;

		P(clazz);

		return lua::class_base::lookup_and_push(L, clazz) ? 1 : 0;
	}

	int all_classes(lua::state& L, lua::parameters& P)
	{
		auto c = lua::class_base::all_classes(L);
		int count = 0;
		for(auto& i : c) {
			L.pushlstring(i);
			count++;
		}
		return count;
	}

	int emulator_ready(lua::state& L, lua::parameters& P)
	{
		L.pushboolean(true);
		return 1;
	}

	int utime(lua::state& L, lua::parameters& P)
	{
		uint64_t t = framerate_regulator::get_utime();
		L.pushnumber(t / 1000000);
		L.pushnumber(t % 1000000);
		return 2;
	}

	int set_idle_timeout(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t dt;

		P(dt);

		core.lua2->idle_hook_time = framerate_regulator::get_utime() + dt;
		return 0;
	}

	int set_timer_timeout(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t dt;

		P(dt);

		core.lua2->timer_hook_time = framerate_regulator::get_utime() + dt;
		return 0;
	}

	int bus_address(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;

		P(addr);

		auto busrange = our_rom.rtype->get_bus_map();
		if(!busrange.second)
			throw std::runtime_error("This platform does not have bus mapping");
		L.pushnumber(busrange.first + (addr % busrange.second));
		return 1;
	}

	int get_lag_flag(lua::state& L, lua::parameters& P)
	{
		L.pushboolean(!(our_rom.rtype && our_rom.rtype->get_pflag()));
		return 1;
	}

	int set_lag_flag(lua::state& L, lua::parameters& P)
	{
		bool flag;

		P(flag);

		if(our_rom.rtype)
			our_rom.rtype->set_pflag(!flag);
		return 0;
	}

	lua::functions misc_fns(lua_func_misc, "", {
		{"print2", print2},
		{"exec", exec},
		{"emulator_ready", emulator_ready},
		{"utime", utime},
		{"set_idle_timeout", set_idle_timeout},
		{"set_timer_timeout", set_timer_timeout},
		{"bus_address", bus_address},
		{"memory.get_lag_flag", get_lag_flag},
		{"memory.set_lag_flag", set_lag_flag},
	});

	lua::functions pure_fns(lua_func_bit, "", {
		{"identify_class", identify_class},
		{"tostringx", tostringx},
		{"lookup_class", lookup_class},
		{"all_classes", all_classes},
	});
}
