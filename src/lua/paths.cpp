#include "lua/internal.hpp"
#include "interface/romtype.hpp"
#include "library/running-executable.hpp"
#include "library/string.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "core/moviedata.hpp"
#include "core/instance.hpp"
#include "core/project.hpp"

namespace
{
	class value_unknown {};
	
	template<std::string(*value)(lua::state& L, lua::parameters& P)>
	int lua_push_string_fn(lua::state& L, lua::parameters& P)
	{
		try {
			L.pushlstring(value(L, P));
		} catch(value_unknown) {
			L.pushnil();
		}
		return 1;
	}

	std::string get_executable_file(lua::state& L, lua::parameters& P)
	{
		try {
			return running_executable();
		} catch(...) {
			throw value_unknown();
		}
	}

	std::string get_executable_path(lua::state& L, lua::parameters& P)
	{
		auto fname = get_executable_file(L, P);
#if defined(__WIN32__) || defined(__WIN64__)
		const char* pathsep = "/\\";
#else
		const char* pathsep = "/";
#endif
		size_t split = fname.find_last_of(pathsep);
		if(split >= fname.length()) throw value_unknown();
		return fname.substr(0, split);
	}

	std::string l_get_config_path(lua::state& L, lua::parameters& P)
	{
		return get_config_path();
	}

	std::string get_rompath(lua::state& L, lua::parameters& P)
	{
		return SET_rompath(*CORE().settings);
	}

	std::string get_firmwarepath(lua::state& L, lua::parameters& P)
	{
		return SET_firmwarepath(*CORE().settings);
	}

	std::string get_slotpath(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		auto p = core.project->get();
		if(p) {
			return p->directory;
		} else {
			return SET_slotpath(*core.settings);
		}
	}

	std::string get_save_slot_file(lua::state& L, lua::parameters& P)
	{
		uint32_t num;
		std::string name;
		bool for_save;
		int binary_flag;
		std::string ret;

		if(P.is_number()) {
			P(num, for_save);
			name = (stringfmt() << "$SLOT:" << num).str();
		} else if(P.is_string()) {
			P(name);
			for_save = false;
		} else {
			(stringfmt() << "Expected string or number as argument #1 to " << P.get_fname()).throwex();
		}
		ret = translate_name_mprefix(name, binary_flag, for_save ? 1 : -1);
		return ret;
	}

	lua::functions LUA_paths_fns(lua_func_misc, "paths", {
		{"get_executable_file", lua_push_string_fn<get_executable_file>},
		{"get_executable_path", lua_push_string_fn<get_executable_path>},
		{"get_config_path", lua_push_string_fn<l_get_config_path>},
		{"get_rompath", lua_push_string_fn<get_rompath>},
		{"get_firmwarepath", lua_push_string_fn<get_firmwarepath>},
		{"get_slotpath", lua_push_string_fn<get_slotpath>},
		{"get_save_slot_file", lua_push_string_fn<get_save_slot_file>},
	});
}
