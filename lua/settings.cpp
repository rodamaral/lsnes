#include "lua-int.hpp"
#include "settings.hpp"

namespace
{
	class lua_settings_set : public lua_function
	{
	public:
		lua_settings_set() : lua_function("settings.set") {}
		int invoke(lua_State* LS, window* win)
		{
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
		}
	} settings_set;

	class lua_settings_get : public lua_function
	{
	public:
		lua_settings_get() : lua_function("settings.get") {}
		int invoke(lua_State* LS, window* win)
		{
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
		}
	} settings_get;

	class lua_settings_blank : public lua_function
	{
	public:
		lua_settings_blank() : lua_function("settings.blank") {}
		int invoke(lua_State* LS, window* win)
		{
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
		}
	} settings_blank;

	class lua_settings_is_set : public lua_function
	{
	public:
		lua_settings_is_set() : lua_function("settings.is_set") {}
		int invoke(lua_State* LS, window* win)
		{
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
		}
	} settings_is_set;
}
