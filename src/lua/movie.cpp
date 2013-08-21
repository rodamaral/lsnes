#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/movie.hpp"
#include "core/rrdata.hpp"
#include "core/moviedata.hpp"
#include "core/mainloop.hpp"

namespace
{
	function_ptr_luafun mcurframe(lua_func_misc, "movie.currentframe", [](lua_state& L, const std::string& fname)
		-> int {
		auto& m = get_movie();
		L.pushnumber(m.get_current_frame());
		return 1;
	});

	function_ptr_luafun mfc(lua_func_misc, "movie.framecount", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		L.pushnumber(m.get_frame_count());
		return 1;
	});

	function_ptr_luafun mrrs(lua_func_misc, "movie.rerecords", [](lua_state& L, const std::string& fname) -> int {
		L.pushnumber(rrdata::count());
		return 1;
	});

	function_ptr_luafun mro(lua_func_misc, "movie.readonly", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		L.pushboolean(m.readonly_mode() ? 1 : 0);
		return 1;
	});

	function_ptr_luafun mrw(lua_func_misc, "movie.readwrite", [](lua_state& L, const std::string& fname) -> int {
		auto& m = get_movie();
		m.readonly_mode(false);
		return 0;
	});

	function_ptr_luafun mfs(lua_func_misc, "movie.frame_subframes", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, "movie.frame_subframes");
		auto& m = get_movie();
		L.pushnumber(m.frame_subframes(frame));
		return 1;
	});

	function_ptr_luafun mrs(lua_func_misc, "movie.read_subframes", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, "movie.frame_subframes");
		uint64_t subframe = L.get_numeric_argument<uint64_t>(2, "movie.frame_subframes");
		auto& m = get_movie();
		controller_frame r = m.read_subframe(frame, subframe);
		L.newtable();

		for(size_t i = 0; i < r.get_index_count(); i++) {
			L.pushnumber(i);
			L.pushnumber(r.axis2(i));
			L.settable(-3);
		}
		return 1;
	});

	function_ptr_luafun rrc(lua_func_misc, "movie.read_rtc", [](lua_state& L, const std::string& fname) -> int {
		L.pushnumber(our_movie.rtc_second);
		L.pushnumber(our_movie.rtc_subsecond);
		return 2;
	});

	function_ptr_luafun musv(lua_func_misc, "movie.unsafe_rewind", [](lua_state& L, const std::string& fname)
		-> int {
		if(L.isnoneornil(1)) {
			//Start process to mark save.
			mainloop_signal_need_rewind(NULL);
		} else if(lua_class<lua_unsaferewind>::is(L, 1)) {
			//Load the save.
			lua_obj_pin<lua_unsaferewind>* u = new lua_obj_pin<lua_unsaferewind>(
				lua_class<lua_unsaferewind>::pin(L, 1, fname.c_str()));
			mainloop_signal_need_rewind(u);
		} else {
			L.pushstring("movie.unsafe_rewind: Expected nil or UNSAFEREWIND as 1st argument");
			L.error();
			return 0;
		}
		return 0;
	});

	function_ptr_luafun movie_to_rewind(lua_func_misc, "movie.to_rewind", [](lua_state& L,
		const std::string& fname) -> int {
		std::string filename = L.get_string(1, fname.c_str());
		moviefile mfile(filename, *our_rom->rtype);
		if(!mfile.is_savestate)
			throw std::runtime_error("movie.to_rewind only allows savestates");
		lua_unsaferewind* u2 = lua_class<lua_unsaferewind>::create(L);
		u2->state = mfile.savestate;
		if(u2->state.size() >= 32)
			u2->state.resize(u2->state.size() - 32);
		u2->secs = mfile.rtc_second;
		u2->ssecs = mfile.rtc_subsecond;
		u2->pollcounters = mfile.pollcounters;
		u2->lag = mfile.lagged_frames;
		u2->frame = mfile.save_frame;
		u2->hostmemory = mfile.host_memory;
		//Now the remaining field ptr is somewhat nastier.
		uint64_t f = 0;
		uint64_t s = mfile.input.size();
		u2->ptr = 0;
		while(++f < u2->frame) {
			if(u2->ptr < s)
				u2->ptr++;
			while(u2->ptr < s && !mfile.input[u2->ptr].sync())
				u2->ptr++;
		}
		return 1;
	});
}
