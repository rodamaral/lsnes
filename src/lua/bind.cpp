#include "core/keymapper.hpp"
#include "core/command.hpp"
#include "lua/internal.hpp"
#include <stdexcept>

namespace
{
	lua::fnptr kbind(lua_func_misc, "keyboard.bind", [](lua::state& L, const std::string& fname) -> int {
		std::string mod = L.get_string(1, fname.c_str());
		std::string mask = L.get_string(2, fname.c_str());
		std::string key = L.get_string(3, fname.c_str());
		std::string cmd = L.get_string(4, fname.c_str());
		lsnes_mapper.bind(mod, mask, key, cmd);
		return 0;
	});

	lua::fnptr kunbind(lua_func_misc, "keyboard.unbind", [](lua::state& L, const std::string& fname)
		-> int {
		std::string mod = L.get_string(1, fname.c_str());
		std::string mask = L.get_string(2, fname.c_str());
		std::string key = L.get_string(3, fname.c_str());
		lsnes_mapper.unbind(mod, mask, key);
		return 0;
	});

	lua::fnptr kalias(lua_func_misc, "keyboard.alias", [](lua::state& L, const std::string& fname)
		-> int {
		std::string alias = L.get_string(1, fname.c_str());
		std::string cmds = L.get_string(2, fname.c_str());
		lsnes_cmd.set_alias_for(alias, cmds);
		refresh_alias_binds();
		return 0;
	});
}
