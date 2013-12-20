#include "lua/internal.hpp"
#include "core/settings.hpp"

namespace
{
	lua::fnptr ss(lua_func_misc, "settings.set", [](lua::state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string value = L.get_string(2, fname.c_str());
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

	lua::fnptr sg(lua_func_misc, "settings.get", [](lua::state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
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
