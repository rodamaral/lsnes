#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/movie.hpp"
#include "core/rrdata.hpp"
#include "core/moviedata.hpp"
#include "core/mainloop.hpp"

namespace
{
	function_ptr_luafun mcurframe(LS, "movie.currentframe", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		L.pushnumber(m.get_current_frame());
		return 1;
	});

	function_ptr_luafun mfc(LS, "movie.framecount", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		L.pushnumber(m.get_frame_count());
		return 1;
	});

	function_ptr_luafun mrrs(LS, "movie.rerecords", [](lua_state& L, const std::string& fname) -> int {
		L.pushnumber(rrdata::count());
		return 1;
	});

	function_ptr_luafun mro(LS, "movie.readonly", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		L.pushboolean(m.readonly_mode() ? 1 : 0);
		return 1;
	});

	function_ptr_luafun mrw(LS, "movie.readwrite", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		m.readonly_mode(false);
		return 0;
	});

	function_ptr_luafun mfs(LS, "movie.frame_subframes", [](lua_state& L, const std::string& fname) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, "movie.frame_subframes");
		auto& m = get_movie();
		L.pushnumber(m.frame_subframes(frame));
		return 1;
	});

	function_ptr_luafun mrs(LS, "movie.read_subframes", [](lua_state& L, const std::string& fname) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, "movie.frame_subframes");
		uint64_t subframe = L.get_numeric_argument<uint64_t>(2, "movie.frame_subframes");
		auto& m = get_movie();
		controller_frame r = m.read_subframe(frame, subframe);
		L.newtable();

		L.pushnumber(0);
		L.pushnumber(r.sync() ? 1 : 0);
		L.settable(-3);
		L.pushnumber(1);
		L.pushnumber(r.axis3(0, 0, 1) ? 1 : 0);
		L.settable(-3);
		L.pushnumber(2);
		L.pushnumber(r.axis3(0, 0, 2));
		L.settable(-3);
		L.pushnumber(3);
		L.pushnumber(r.axis3(0, 0, 3));
		L.settable(-3);

		for(size_t i = 4; i < r.get_index_count(); i++) {
			L.pushnumber(i);
			L.pushnumber(r.axis2(i));
			L.settable(-3);
		}
		return 1;
	});

	function_ptr_luafun rrc(LS, "movie.read_rtc", [](lua_state& L, const std::string& fname) -> int {
		L.pushnumber(our_movie.rtc_second);
		L.pushnumber(our_movie.rtc_subsecond);
		return 2;
	});

	function_ptr_luafun musv(LS, "movie.unsafe_rewind", [](lua_state& L, const std::string& fname) -> int {
		if(L.isnoneornil(1)) {
			//Start process to mark save.
			mainloop_signal_need_rewind(NULL);
		} else if(lua_class<lua_unsaferewind>::is(L, 1)) {
			//Load the save.
			lua_obj_pin<lua_unsaferewind>* u = lua_class<lua_unsaferewind>::pin(L, 1, fname.c_str());
			mainloop_signal_need_rewind(u);
		} else {
			L.pushstring("movie.unsafe_rewind: Expected nil or UNSAFEREWIND as 1st argument");
			L.error();
			return 0;
		}
	});
}

