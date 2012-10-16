#include "lua/internal.hpp"
#include "core/settings.hpp"

namespace
{
	function_ptr_luafun ss(LS, "settings.set", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string value = L.get_string(2, fname.c_str());
		try {
			lsnes_set.set(name, value);
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
		L.pushboolean(1);
		return 1;
	});

	function_ptr_luafun sg(LS, "settings.get", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		try {
			if(!lsnes_set.is_set(name))
				L.pushboolean(0);
			else {
				std::string value = lsnes_set.get(name);
				L.pushlstring(value.c_str(), value.length());
			}
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	});

	function_ptr_luafun sb(LS, "settings.blank", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		try {
			lsnes_set.blank(name);
			L.pushboolean(1);
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	});

	function_ptr_luafun si(LS, "settings.is_set", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		try {
			bool x = lsnes_set.is_set(name);
			L.pushboolean(x ? 1 : 0);
			return 1;
		} catch(std::exception& e) {
			L.pushnil();
			L.pushstring(e.what());
			return 2;
		}
	});
}
