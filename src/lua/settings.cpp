#include "lua/internal.hpp"
#include "core/instance.hpp"
#include "core/framerate.hpp"
#include "library/settingvar.hpp"
#include <limits>

namespace
{
	int ss_set(lua::state& L, lua::parameters& P)
	{
		std::string name, value;

		P(name, value);

		try {
			CORE().setcache->set(name, value);
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
		L.pushboolean(1);
		return 1;
	}

	int ss_get(lua::state& L, lua::parameters& P)
	{
		std::string name;

		P(name);

		try {
			std::string value = CORE().setcache->get(name);
			L.pushlstring(value.c_str(), value.length());
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	}

	int ss_getlist(lua::state& L, lua::parameters& P)
	{
		L.newtable();
		auto& settings = *CORE().settings;
		auto set = settings.get_settings_set();
		for(auto i : set) {
			auto& setting = settings[i];
			L.pushlstring(setting.get_iname());
			L.pushlstring(setting.str());
			L.settable(-3);
		}
		return 1;
	}

	int ss_getspeed(lua::state& L, lua::parameters& P)
	{
		double spd = CORE().framerate->get_speed_multiplier();
		if(spd == std::numeric_limits<double>::infinity())
			L.pushstring("turbo");
		else
			L.pushnumber(spd);
		return 1;
	}

	int ss_setspeed(lua::state& L, lua::parameters& P)
	{
		double spd = 0;
		std::string special;
		bool is_string = false;

		if(P.is_string()) {
			P(special);
			is_string = true;
		} else {
			P(spd);
		}
		if(special == "turbo")
			CORE().framerate->set_speed_multiplier(std::numeric_limits<double>::infinity());
		else if(!is_string && spd > 0)
			CORE().framerate->set_speed_multiplier(spd);
		else
			throw std::runtime_error("Unknown special speed");
		return 0;
	}

	lua::functions LUA_settings_fns(lua_func_misc, "settings", {
		{"set", ss_set},
		{"get", ss_get},
		{"get_all", ss_getlist},
		{"get_speed", ss_getspeed},
		{"set_speed", ss_setspeed},
	});
}
