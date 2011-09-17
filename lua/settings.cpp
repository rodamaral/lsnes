#include "lua-int.hpp"
#include "settings.hpp"

namespace
{
	function_ptr_luafun ss("settings.set", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::string value = get_string_argument(LS, 2, fname.c_str());
		try {
			setting::set(name, value);
		} catch(std::exception& e) {
			lua_pushnil(LS);
			lua_pushstring(LS, e.what());
			return 2;
		}
		lua_pushboolean(LS, 1);
		return 1;
	});

	function_ptr_luafun sg("settings.get", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		try {
			if(!setting::is_set(name))
				lua_pushboolean(LS, 0);
			else {
				std::string value = setting::get(name);
				lua_pushlstring(LS, value.c_str(), value.length());
			}
			return 1;
		} catch(std::exception& e) {
			lua_pushnil(LS);
			lua_pushstring(LS, e.what());
			return 2;
		}
	});

	function_ptr_luafun sb("settings.blank", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		try {
			setting::blank(name);
			lua_pushboolean(LS, 1);
			return 1;
		} catch(std::exception& e) {
			lua_pushnil(LS);
			lua_pushstring(LS, e.what());
			return 2;
		}
	});

	function_ptr_luafun si("settings.is_set", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		try {
			bool x = setting::is_set(name);
			lua_pushboolean(LS, x ? 1 : 0);
			return 1;
		} catch(std::exception& e) {
			lua_pushnil(LS);
			lua_pushstring(LS, e.what());
			return 2;
		}
	});
}
