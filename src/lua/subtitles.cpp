#include "lua/internal.hpp"
#include "core/instance.hpp"

namespace
{
	int sub_byindex(lua::state& L, lua::parameters& P)
	{
		auto n = P.arg<uint64_t>();
		uint64_t j = 0;
		for(auto i : lsnes_instance.subtitles.get_all()) {
			if(j == n) {
				L.pushnumber(i.first);
				L.pushnumber(i.second);
				return 2;
			}
			j++;
		}
		return 0;
	}

	int sub_get(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string x = lsnes_instance.subtitles.get(frame, length);
		L.pushstring(x.c_str());
		return 1;
	}

	int sub_set(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string text = P.arg<std::string>();
		lsnes_instance.subtitles.set(frame, length, text);
		return 0;
	}

	int sub_delete(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		lsnes_instance.subtitles.set(frame, length, "");
		return 0;
	}

	lua::functions lua_subtitles(lua_func_misc, "subtitle", {
		{"byindex", sub_byindex},
		{"get", sub_get},
		{"set", sub_set},
		{"delete", sub_delete},
	});
}
