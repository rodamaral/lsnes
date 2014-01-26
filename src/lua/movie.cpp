#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/movie.hpp"
#include "core/rrdata.hpp"
#include "core/moviedata.hpp"
#include "core/mainloop.hpp"

namespace
{
	lua::fnptr2 mcurframe(lua_func_misc, "movie.currentframe", [](lua::state& L, lua::parameters& P) -> int {
		auto& m = get_movie();
		L.pushnumber(m.get_current_frame());
		return 1;
	});

	lua::fnptr2 mfc(lua_func_misc, "movie.framecount", [](lua::state& L, lua::parameters& P) -> int {
		auto& m = get_movie();
		L.pushnumber(m.get_frame_count());
		return 1;
	});

	lua::fnptr2 mrrs(lua_func_misc, "movie.rerecords", [](lua::state& L, lua::parameters& P) -> int {
		L.pushnumber(rrdata.count());
		return 1;
	});

	lua::fnptr2 mro(lua_func_misc, "movie.readonly", [](lua::state& L, lua::parameters& P) -> int {
		auto& m = get_movie();
		L.pushboolean(m.readonly_mode() ? 1 : 0);
		return 1;
	});

	lua::fnptr2 mrw(lua_func_misc, "movie.readwrite", [](lua::state& L, lua::parameters& P) -> int {
		auto& m = get_movie();
		m.readonly_mode(false);
		return 0;
	});

	lua::fnptr2 mfs(lua_func_misc, "movie.frame_subframes", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t frame = L.get_numeric_argument<uint64_t>(1, "movie.frame_subframes");
		auto& m = get_movie();
		L.pushnumber(m.frame_subframes(frame));
		return 1;
	});

	lua::fnptr2 mrs(lua_func_misc, "movie.read_subframes", [](lua::state& L, lua::parameters& P) -> int {
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

	lua::fnptr2 rrc(lua_func_misc, "movie.read_rtc", [](lua::state& L, lua::parameters& P) -> int {
		L.pushnumber(our_movie.rtc_second);
		L.pushnumber(our_movie.rtc_subsecond);
		return 2;
	});

	lua::fnptr2 musv(lua_func_misc, "movie.unsafe_rewind", [](lua::state& L, lua::parameters& P) -> int {
		if(P.is_novalue()) {
			//Start process to mark save.
			mainloop_signal_need_rewind(NULL);
		} else if(P.is<lua_unsaferewind>()) {
			//Load the save.
			lua::objpin<lua_unsaferewind>* u = new lua::objpin<lua_unsaferewind>(
				P.arg<lua::objpin<lua_unsaferewind>>());
			mainloop_signal_need_rewind(u);
		} else
			P.expected("UNSAFEREWIND or nil");
		return 0;
	});

	lua::fnptr2 movie_to_rewind(lua_func_misc, "movie.to_rewind", [](lua::state& L, lua::parameters& P) -> int {
		auto filename = P.arg<std::string>();
		moviefile mfile(filename, *our_rom.rtype);
		if(!mfile.is_savestate)
			throw std::runtime_error("movie.to_rewind only allows savestates");
		lua_unsaferewind* u2 = lua::_class<lua_unsaferewind>::create(L);
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
