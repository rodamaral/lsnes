#include "lua/internal.hpp"
#include "core/subtitles.hpp"

namespace
{
	lua::fnptr2 enumerate(lua_func_misc, "subtitle.byindex", [](lua::state& L, lua::parameters& P) -> int {
		auto n = P.arg<uint64_t>();
		uint64_t j = 0;
		for(auto i : get_subtitles()) {
			if(j == n) {
				L.pushnumber(i.first);
				L.pushnumber(i.second);
				return 2;
			}
			j++;
		}
		return 0;
	});

	lua::fnptr2 sget(lua_func_misc, "subtitle.get", [](lua::state& L, lua::parameters& P) -> int {
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string x = get_subtitle_for(frame, length);
		L.pushstring(x.c_str());
		return 1;
	});

	lua::fnptr2 sset(lua_func_misc, "subtitle.set", [](lua::state& L, lua::parameters& P) -> int {
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string text = P.arg<std::string>();
		set_subtitle_for(frame, length, text);
		return 0;
	});

	lua::fnptr2 sdel(lua_func_misc, "subtitle.delete", [](lua::state& L, lua::parameters& P) -> int {
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		set_subtitle_for(frame, length, "");
		return 0;
	});
}
