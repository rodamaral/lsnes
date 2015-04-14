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
		L.pushnumber(core.mlogic->get_mfile().dyn.rtc_second);
		L.pushnumber(core.mlogic->get_mfile().dyn.rtc_subsecond);
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
		if(!mfile.dyn.is_savestate)
			throw std::runtime_error("movie.to_rewind only allows savestates");
		lua_unsaferewind* u2 = lua::_class<lua_unsaferewind>::create(L);
		u2->state = mfile.dyn.savestate;
		if(u2->state.size() >= 32)
			u2->state.resize(u2->state.size() - 32);
		u2->secs = mfile.dyn.rtc_second;
		u2->ssecs = mfile.dyn.rtc_subsecond;
		u2->pollcounters = mfile.dyn.pollcounters;
		u2->lag = mfile.dyn.lagged_frames;
		u2->frame = mfile.dyn.save_frame;
		u2->hostmemory = mfile.dyn.host_memory;
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

	int rom_loaded(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		L.pushboolean(!core.rom->get_internal_rom_type().isnull());
		return 1;
	}

	int get_rom_info(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		auto& rom = *(core.rom);
		bool any_loaded = false;

		L.newtable();
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			auto img = &rom.get_rom(i);
			auto xml = &rom.get_markup(i);
			if(img->sha_256.read() == "") img = NULL;	//Trivial image.
			if(xml->sha_256.read() == "") xml = NULL;	//Trivial image.
			if(!img && !xml) continue;
			any_loaded = true;
			L.pushnumber(i+1);
			L.newtable();
			if(img) {
				L.pushstring("filename");
				L.pushlstring(img->filename);
				L.rawset(-3);
				L.pushstring("hint");
				L.pushlstring(img->namehint);
				L.rawset(-3);
				L.pushstring("sha256");
				L.pushlstring(img->sha_256.read());
				L.rawset(-3);
			}
			if(xml) {
				L.pushstring("xml_filename");
				L.pushlstring(xml->filename);
				L.rawset(-3);
				L.pushstring("xml_hint");
				L.pushlstring(xml->namehint);
				L.rawset(-3);
				L.pushstring("xml_sha256");
				L.pushlstring(xml->sha_256.read());
				L.rawset(-3);
			}
			L.rawset(-3);
		}
		if(!any_loaded) {
			L.pop(1);
			return 0;
		}
		return 1;
	}

	int get_game_info(lua::state& L, lua::parameters& P)
	{
		uint64_t framemagic[4];
		auto& core = CORE();
		auto& rom = core.rom->get_internal_rom_type();
		auto& sysreg = rom.combine_region(rom.get_region());

		sysreg.fill_framerate_magic(framemagic);

		L.newtable();

		L.pushstring("core");
		L.pushlstring(rom.get_core_identifier());
		L.rawset(-3);
		L.pushstring("core_short");
		L.pushlstring(rom.get_core_shortname());
		L.rawset(-3);
		L.pushstring("type");
		L.pushlstring(rom.get_iname());
		L.rawset(-3);
		L.pushstring("type_long");
		L.pushlstring(rom.get_hname());
		L.rawset(-3);
		L.pushstring("region");
		L.pushlstring(rom.get_region().get_iname());
		L.rawset(-3);
		L.pushstring("region_long");
		L.pushlstring(rom.get_region().get_hname());
		L.rawset(-3);
		L.pushstring("gametype");
		L.pushlstring(sysreg.get_name());
		L.rawset(-3);
		L.pushstring("fps_n");
		L.pushnumber(framemagic[1]);
		L.rawset(-3);
		L.pushstring("fps_d");
		L.pushnumber(framemagic[0]);
		L.rawset(-3);
		L.pushstring("fps");
		L.pushnumber(1.0 * framemagic[1] / framemagic[0]);
		L.rawset(-3);

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
		{"rom_loaded", rom_loaded},
		{"get_rom_info", get_rom_info},
		{"get_game_info", get_game_info},
	});
}
