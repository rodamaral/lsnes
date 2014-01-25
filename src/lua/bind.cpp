#include "core/keymapper.hpp"
#include "core/command.hpp"
#include "lua/internal.hpp"
#include <stdexcept>

namespace
{
	lua::fnptr2 kbind(lua_func_misc, "keyboard.bind", [](lua::state& L, lua::parameters& P) -> int {
		auto mod = P.arg<std::string>();
		auto mask = P.arg<std::string>();
		auto key = P.arg<std::string>();
		auto cmd = P.arg<std::string>();
		lsnes_mapper.bind(mod, mask, key, cmd);
		return 0;
	});

	lua::fnptr2 kunbind(lua_func_misc, "keyboard.unbind", [](lua::state& L, lua::parameters& P)
		-> int {
		auto mod = P.arg<std::string>();
		auto mask = P.arg<std::string>();
		auto key = P.arg<std::string>();
		lsnes_mapper.unbind(mod, mask, key);
		return 0;
	});

	lua::fnptr2 kalias(lua_func_misc, "keyboard.alias", [](lua::state& L, lua::parameters& P)
		-> int {
		auto alias = P.arg<std::string>();
		auto cmds = P.arg<std::string>();
		lsnes_cmd.set_alias_for(alias, cmds);
		refresh_alias_binds();
		return 0;
	});
}
