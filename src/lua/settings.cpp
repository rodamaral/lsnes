#include "lua/internal.hpp"
#include "core/settings.hpp"

namespace
{
	int ss_set(lua::state& L, lua::parameters& P)
	{
		std::string name, value;

		P(name, value);

		try {
			lsnes_vsetc.set(name, value);
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
			std::string value = lsnes_vsetc.get(name);
			L.pushlstring(value.c_str(), value.length());
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	}

	class lua_settings_dummy {};
	lua::_class<lua_settings_dummy> lua_settings(lua_class_bind, "*settings", {
		{"set", ss_set},
		{"get", ss_get},
	});
}
