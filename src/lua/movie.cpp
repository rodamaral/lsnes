#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"

namespace
{
	function_ptr_luafun mcurframe("movie.currentframe", [](lua_State* LS, const std::string& fname) -> int {
		auto& m = get_movie();
		lua_pushnumber(LS, m.get_current_frame());
		return 1;
	});

	function_ptr_luafun mfc("movie.framecount", [](lua_State* LS, const std::string& fname) -> int {
		auto& m = get_movie();
		lua_pushnumber(LS, m.get_frame_count());
		return 1;
	});

	function_ptr_luafun mro("movie.readonly", [](lua_State* LS, const std::string& fname) -> int {
		auto& m = get_movie();
		lua_pushboolean(LS, m.readonly_mode() ? 1 : 0);
		return 1;
	});

	function_ptr_luafun mrw("movie.readwrite", [](lua_State* LS, const std::string& fname) -> int {
		auto& m = get_movie();
		m.readonly_mode(false);
		return 0;
	});

	function_ptr_luafun mfs("movie.frame_subframes", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
		auto& m = get_movie();
		lua_pushnumber(LS, m.frame_subframes(frame));
		return 1;
	});

	function_ptr_luafun mrs("movie.read_subframes", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
		uint64_t subframe = get_numeric_argument<uint64_t>(LS, 2, "movie.frame_subframes");
		auto& m = get_movie();
		controller_frame r = m.read_subframe(frame, subframe);
		lua_newtable(LS);

		lua_pushnumber(LS, 0);
		lua_pushnumber(LS, r.sync() ? 1 : 0);
		lua_settable(LS, -3);
		lua_pushnumber(LS, 1);
		lua_pushnumber(LS, r.reset() ? 1 : 0);
		lua_settable(LS, -3);
		lua_pushnumber(LS, 2);
		lua_pushnumber(LS, r.delay().first);
		lua_settable(LS, -3);
		lua_pushnumber(LS, 3);
		lua_pushnumber(LS, r.delay().second);
		lua_settable(LS, -3);

		for(size_t i = 0; i < r.maxbuttons(); i++) {
			lua_pushnumber(LS, i + 4);
			lua_pushnumber(LS, r.axis2(i));
			lua_settable(LS, -3);
		}
		return 1;
	});

	function_ptr_luafun rrc("movie.read_rtc", [](lua_State* LS, const std::string& fname) -> int {
		lua_pushnumber(LS, our_movie.rtc_second);
		lua_pushnumber(LS, our_movie.rtc_subsecond);
		return 2;
	});
}
