#include "lua/internal.hpp"
#include "core/keymapper.hpp"
#include "core/command.hpp"
#include <vector>

class lua_inverse_bind
{
public:
	lua_inverse_bind(const std::string name, const std::string cmd);
private:
	inverse_bind ikey;
};

lua_inverse_bind::lua_inverse_bind(const std::string name, const std::string cmd)
	: ikey(lsnes_mapper, cmd, "Lua‣" + name)
{
}

namespace
{
	function_ptr_luafun input_bindings(LS, "list_bindings", [](lua_state& L, const std::string& fname) -> int {
		std::string target;
		if(!L.isnoneornil(1))
			target = L.get_string(1, fname.c_str());
		L.newtable();
		for(auto key : lsnes_mapper.get_bindings()) {
			std::string _key = key;
			std::string cmd = lsnes_mapper.get(key);
			if(target != "" && cmd != target)
				continue;
			L.pushlstring(_key.c_str(), _key.length());
			L.pushlstring(cmd.c_str(), cmd.length());
			L.rawset(-3);
		}
		for(auto key : lsnes_mapper.get_controller_keys()) {
			std::string _key = key->get_string();
			if(_key == "")
				continue;
			std::string cmd = key->get_command();
			_key = "|/" + _key;
			if(target != "" && cmd != target)
				continue;
			L.pushlstring(_key.c_str(), _key.length());
			L.pushlstring(cmd.c_str(), cmd.length());
			L.rawset(-3);
			
		}
		return 1;
	});

	function_ptr_luafun get_alias(LS, "get_alias", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string a = lsnes_cmd.get_alias_for(name);
		if(a != "")
			L.pushlstring(a.c_str(), a.length());
		else
			L.pushnil();
		return 1;
	});

	function_ptr_luafun set_alias(LS, "set_alias", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string value;
		if(L.type(2) != LUA_TNIL)
			value = L.get_string(2, fname.c_str());
		lsnes_cmd.set_alias_for(name, value);
		return 0;
	});

	function_ptr_luafun create_ibind(LS, "create_ibind", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string command = L.get_string(2, fname.c_str());
		lua_inverse_bind* b = lua_class<lua_inverse_bind>::create(L, name, command);
		return 1;
	});
}

DECLARE_LUACLASS(lua_inverse_bind, "INVERSEBIND");
