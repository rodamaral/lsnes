#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/mainloop.hpp"

namespace
{
	int currentframe(lua::state& L, lua::parameters& P)
	{
		auto& m = CORE().mlogic->get_movie();
		L.pushnumber(m.get_current_frame());
		return 1;
	}

	int lagcounter(lua::state& L, lua::parameters& P)
	{
		auto& m = CORE().mlogic->get_movie();
		L.pushnumber(m.get_lag_frames());
		return 1;
	}

	int framecount(lua::state& L, lua::parameters& P)
	{
		auto& m = CORE().mlogic->get_movie();
		L.pushnumber(m.get_frame_count());
		return 1;
	}

	int rerecords(lua::state& L, lua::parameters& P)
	{
		L.pushnumber(CORE().mlogic->get_rrdata().count());
		return 1;
	}

	int readonly(lua::state& L, lua::parameters& P)
	{
		auto& m = CORE().mlogic->get_movie();
		L.pushboolean(m.readonly_mode() ? 1 : 0);
		return 1;
	}

	int readwrite(lua::state& L, lua::parameters& P)
	{
		auto& m = CORE().mlogic->get_movie();
		m.readonly_mode(false);
		return 0;
	}

	int frame_subframes(lua::state& L, lua::parameters& P)
	{
		uint64_t frame;

		P(frame);

		auto& m = CORE().mlogic->get_movie();
		L.pushnumber(m.frame_subframes(frame));
		return 1;
	}

	int read_subframes(lua::state& L, lua::parameters& P)
	{
		uint64_t frame, subframe;

		P(frame, subframe);

		auto& m = CORE().mlogic->get_movie();
		portctrl::frame r = m.read_subframe(frame, subframe);
		L.newtable();

		for(size_t i = 0; i < r.get_index_count(); i++) {
			L.pushnumber(i);
			L.pushnumber(r.axis2(i));
			L.settable(-3);
		}
		return 1;
	}

	int read_rtc(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		L.pushnumber(core.mlogic->get_mfile().rtc_second);
		L.pushnumber(core.mlogic->get_mfile().rtc_subsecond);
		return 2;
	}

	int unsafe_rewind(lua::state& L, lua::parameters& P)
	{
		if(P.is_novalue()) {
			//Start process to mark save.
			mainloop_signal_need_rewind(NULL);
		} else if(P.is<lua_unsaferewind>()) {
			//Load the save.
			lua::objpin<lua_unsaferewind> pin;

			P(pin);

			mainloop_signal_need_rewind(new lua::objpin<lua_unsaferewind>(pin));
		} else
			P.expected("UNSAFEREWIND or nil");
		return 0;
	}

	int to_rewind(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string filename;

		P(filename);

		moviefile mfile(filename, core.rom->get_internal_rom_type());
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
		uint64_t s = mfile.input->size();
		u2->ptr = 0;
		while(++f < u2->frame) {
			if(u2->ptr < s)
				u2->ptr++;
			while(u2->ptr < s && !(*mfile.input)[u2->ptr].sync())
				u2->ptr++;
		}
		return 1;
	}

	lua::functions LUA_movie_fns(lua_func_misc, "movie", {
		{"currentframe", currentframe},
		{"lagcount", lagcounter},
		{"framecount", framecount},
		{"rerecords", rerecords},
		{"readonly", readonly},
		{"readwrite", readwrite},
		{"frame_subframes", frame_subframes},
		{"read_subframes", read_subframes},
		{"read_rtc", read_rtc},
		{"unsafe_rewind", unsafe_rewind},
		{"to_rewind", to_rewind},
	});
}
