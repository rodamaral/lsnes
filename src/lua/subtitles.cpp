#include "lua/internal.hpp"
#include "core/subtitles.hpp"

namespace
{
	function_ptr_luafun enumerate("subtitle.byindex", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t n = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t j = 0;
		for(auto i : get_subtitles()) {
			if(j == n) {
				lua_pushnumber(LS, i.first);
				lua_pushnumber(LS, i.second);
				return 2;
			}
			j++;
		}
		return 0;
	});

	function_ptr_luafun sget("subtitle.get", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t length = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		std::string x = get_subtitle_for(frame, length);
		lua_pushstring(LS, x.c_str());
		return 1;
	});

	function_ptr_luafun sset("subtitle.set", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t length = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		std::string text = get_string_argument(LS, 3, fname.c_str());
		set_subtitle_for(frame, length, text);
		return 0;
	});

	function_ptr_luafun sdel("subtitle.delete", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t length = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		set_subtitle_for(frame, length, "");
		return 0;
	});
}
