#include "lua/internal.hpp"
#include "core/settings.hpp"

namespace
{
	lua::fnptr2 ss(lua_func_misc, "settings.set", [](lua::state& L, lua::parameters& P) -> int {
		auto name = P.arg<std::string>();
		auto value = P.arg<std::string>();
		try {
			lsnes_vsetc.set(name, value);
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
		L.pushboolean(1);
		return 1;
	});

	lua::fnptr2 sg(lua_func_misc, "settings.get", [](lua::state& L, lua::parameters& P) -> int {
		auto name = P.arg<std::string>();
		try {
			std::string value = lsnes_vsetc.get(name);
			L.pushlstring(value.c_str(), value.length());
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	});
}
