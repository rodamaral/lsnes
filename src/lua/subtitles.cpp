#include "lua/internal.hpp"
#include "core/subtitles.hpp"

namespace
{
	function_ptr_luafun enumerate(lua_func_misc, "subtitle.byindex", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t n = L.get_numeric_argument<uint64_t>(1, fname.c_str());
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

	function_ptr_luafun sget(lua_func_misc, "subtitle.get", [](lua_state& L, const std::string& fname) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t length = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		std::string x = get_subtitle_for(frame, length);
		L.pushstring(x.c_str());
		return 1;
	});

	function_ptr_luafun sset(lua_func_misc, "subtitle.set", [](lua_state& L, const std::string& fname) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t length = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		std::string text = L.get_string(3, fname.c_str());
		set_subtitle_for(frame, length, text);
		return 0;
	});

	function_ptr_luafun sdel(lua_func_misc, "subtitle.delete", [](lua_state& L, const std::string& fname) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t length = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		set_subtitle_for(frame, length, "");
		return 0;
	});
}
