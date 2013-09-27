#include "core/keymapper.hpp"
#include "core/command.hpp"
#include "lua/internal.hpp"
#include <stdexcept>

namespace
{
	function_ptr_luafun kbind(lua_func_misc, "keyboard.bind", [](lua_state& L, const std::string& fname) -> int {
		std::string mod = L.get_string(1, fname.c_str());
		std::string mask = L.get_string(2, fname.c_str());
		std::string key = L.get_string(3, fname.c_str());
		std::string cmd = L.get_string(4, fname.c_str());
		lsnes_mapper.bind(mod, mask, key, cmd);
		return 0;
	});

	function_ptr_luafun kunbind(lua_func_misc, "keyboard.unbind", [](lua_state& L, const std::string& fname)
		-> int {
		std::string mod = L.get_string(1, fname.c_str());
		std::string mask = L.get_string(2, fname.c_str());
		std::string key = L.get_string(3, fname.c_str());
		lsnes_mapper.unbind(mod, mask, key);
		return 0;
	});

	function_ptr_luafun kalias(lua_func_misc, "keyboard.alias", [](lua_state& L, const std::string& fname)
		-> int {
		std::string alias = L.get_string(1, fname.c_str());
		std::string cmds = L.get_string(2, fname.c_str());
		lsnes_cmd.set_alias_for(alias, cmds);
		refresh_alias_binds();
		return 0;
	});
}
