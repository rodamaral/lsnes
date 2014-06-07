#include "lua/internal.hpp"
#include "core/instance.hpp"
#include "library/settingvar.hpp"

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

	lua::functions LUA_settings_fns(lua_func_misc, "settings", {
		{"set", ss_set},
		{"get", ss_get},
	});
}
