#include "lua/internal.hpp"
#include "core/subtitles.hpp"

namespace
{
	int sub_byindex(lua::state& L, lua::parameters& P)
	{
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
	}

	int sub_get(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string x = get_subtitle_for(frame, length);
		L.pushstring(x.c_str());
		return 1;
	}

	int sub_set(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		std::string text = P.arg<std::string>();
		set_subtitle_for(frame, length, text);
		return 0;
	}

	int sub_delete(lua::state& L, lua::parameters& P)
	{
		auto frame = P.arg<uint64_t>();
		auto length = P.arg<uint64_t>();
		set_subtitle_for(frame, length, "");
		return 0;
	}

	class lua_subtitles_dummy {};
	lua::_class<lua_subtitles_dummy> lua_subtitles(lua_class_bind, "*subtitle", {
		{"byindex", sub_byindex},
		{"get", sub_get},
		{"set", sub_set},
		{"delete", sub_delete},
	});
}
