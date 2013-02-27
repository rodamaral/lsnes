#include "lua/internal.hpp"
#include "core/keymapper.hpp"
#include "core/command.hpp"
#include <vector>

class lua_inverse_bind
{
public:
	lua_inverse_bind(const std::string name, const std::string cmd);
private:
	inverse_key ikey;
};

lua_inverse_bind::lua_inverse_bind(const std::string name, const std::string cmd)
	: ikey(cmd, "Lua‣" + name)
{
}

namespace
{
	function_ptr_luafun input_bindings("list_bindings", [](lua_State* LS, const std::string& fname) -> int {
		std::string target;
		if(!lua_isnoneornil(LS, 1))
			target = get_string_argument(LS, 1, fname.c_str());
		lua_newtable(LS);
		for(auto key : keymapper::get_bindings()) {
			std::string cmd = keymapper::get_command_for(key);
			if(target != "" && cmd != target)
				continue;
			lua_pushlstring(LS, key.c_str(), key.length());
			lua_pushlstring(LS, cmd.c_str(), cmd.length());
			lua_rawset(LS, -3);
		}
		return 1;
	});
	
	function_ptr_luafun get_alias("get_alias", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::string a = command::get_alias_for(name);
		if(a != "")
			lua_pushlstring(LS, a.c_str(), a.length());
		else
			lua_pushnil(LS);
		return 1;
	});

	function_ptr_luafun set_alias("set_alias", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::string value;
		if(lua_type(LS, 2) != LUA_TNIL)
			value = get_string_argument(LS, 2, fname.c_str());
		command::set_alias_for(name, value);
		return 0;
	});

	function_ptr_luafun create_ibind("create_ibind", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::string command = get_string_argument(LS, 2, fname.c_str());
		lua_inverse_bind* b = lua_class<lua_inverse_bind>::create(LS, name, command);
		return 1;
	});
}

DECLARE_LUACLASS(lua_inverse_bind, "INVERSEBIND");
