#include "logic.hpp"
#include "framebuffer.hpp"
#include "music.hpp"
#include "messages.hpp"
#include "romimage.hpp"
#include "draw.hpp"
#include "instance.hpp"
#include "core/messages.hpp"
#include "library/zip.hpp"

#define DEMO_WAIT 1080

namespace sky
{
	const uint8_t hash1[32] = {
		66,234,24,74,51,217,34,32,61,153,253,130,17,157,160,183,62,231,155,167,135,216,249,116,41,95,216,
		97,168,163,129,23
	};
	const uint8_t hash2[32] = {
		68,57,232,117,27,127,113,87,161,78,208,193,235,97,13,131,227,152,229,127,31,114,47,235,97,248,103,
		11,159,217,129,136
	};

	void reload_song(struct instance& inst, bool menu, uint32_t num)
	{
		if(menu) {
			inst.mplayer.set_song(NULL);
			if(inst.bsong)
				delete inst.bsong;
			inst.bsong = NULL;
			std::string filename = inst.rom_filename + "/menu.opus";
			std::istream* s = NULL;
			try {
				s = &zip::openrel(filename, "");
				messages << "Selected song: " << filename << std::endl;
				inst.bsong = new song_buffer(*s);
				inst.mplayer.set_song(inst.bsong);
				delete s;
			} catch(std::exception& e) {
				if(s) {
					messages << "Unable to load song: " << e.what() << std::endl;
					delete s;
				}
			}
		} else {
			inst.mplayer.set_song(NULL);
			if(inst.bsong)
				delete inst.bsong;
			inst.bsong = NULL;
			std::istream* s = NULL;
			try {
				zip::reader r(inst.rom_filename);
				std::string iname;
				std::vector<std::string> inames;
				for(auto i : r)
					if(regex_match("music.+\\.opus", i))
						inames.push_back(i);
				if(inames.empty())
					return;
				iname = inames[num % inames.size()];
				s = &r[iname];
				messages << "Selected song: " << iname << std::endl;
				inst.bsong = new song_buffer(*s);
				inst.mplayer.set_song(inst.bsong);
				delete s;
			} catch(std::exception& e) {
				messages << "Unable to load song: " << e.what() << std::endl;
				if(s)
					delete s;
			}
		}
	}

	void faded_framebuffer(struct instance& inst, uint16_t alpha)
	{
		inst.indirect_flag = true;
		for(unsigned i = 0; i < sizeof(inst.framebuffer) / sizeof(inst.framebuffer[0]); i++) {
			uint32_t L = (((inst.framebuffer[i] & 0x00FF00FF) * alpha) >> 8) & 0x00FF00FF;
			uint32_t H = (((inst.framebuffer[i] & 0xFF00FF00U) >> 8) * alpha) & 0xFF00FF00U;
			inst.fadeffect_buffer[i] = L | H;
		}
	}

	uint16_t stage_to_xpos(uint8_t stage)
	{
		return (stage > 15) ? 224 : 64;
	}

	uint16_t stage_to_ypos(uint8_t stage)
	{
		return 13 + (((stage - 1) % 15) / 3) * 39 + ((stage - 1) % 3) * 9;;
	}

	uint8_t do_menu_fadein(struct instance& inst, uint16_t b)
	{
		faded_framebuffer(inst, 8 * inst.state.fadecount);
		return (++inst.state.fadecount == 32) ? state_menu : state_menu_fadein;
	}

	uint8_t do_menu_fadeout(struct instance& inst, uint16_t b)
	{
		faded_framebuffer(inst, 256 - 8 * inst.state.fadecount);
		return (++inst.state.fadecount == 32) ? state_load_level : state_menu_fadeout;
	}

	uint8_t do_level_fadein(struct instance& inst, uint16_t b)
	{
		uint8_t fadelimit = inst.state.timeattack ? 144 : 32;
		faded_framebuffer(inst, 256 * inst.state.fadecount / fadelimit);
		return (++inst.state.fadecount == fadelimit) ? state_level_play : state_level_fadein;
	}

	uint8_t do_level_fadeout(struct instance& inst, uint16_t b)
	{
		faded_framebuffer(inst, 256 - 8 * inst.state.fadecount);
		return (++inst.state.fadecount == 32) ? state_load_menu : state_level_fadeout;
	}

	uint8_t do_level_fadeout_retry(struct instance& inst, uint16_t b)
	{
		faded_framebuffer(inst, 256 - 8 * inst.state.fadecount);
		return (++inst.state.fadecount == 32) ? state_load_level_nomus : state_level_fadeout_retry;
	}

	uint8_t do_level_unavail(struct instance& inst, uint16_t b)
	{
		inst.indirect_flag = false;
		if(!inst.state.fadecount) {
			memset(inst.framebuffer, 0, sizeof(inst.framebuffer));
			memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
			draw_message(inst, _lvlunavail_g, 0xFFFFFF, 0);
		}
		inst.state.fadecount = 1;
		return ((b & ~inst.state.lastkeys) & 0x30) ? state_load_menu : state_level_unavail;
	}

	uint8_t do_demo_unavail(struct instance& inst, uint16_t b)
	{
		inst.indirect_flag = false;
		if(!inst.state.fadecount) {
			memset(inst.framebuffer, 0, sizeof(inst.framebuffer));
			memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
			draw_message(inst, _demounavail_g, 0xFFFFFF, 0);
		}
		inst.state.fadecount = 1;
		return ((b & ~inst.state.lastkeys) & 0x30) ? state_load_menu : state_demo_unavail;
	}

	uint8_t do_level_complete(struct instance& inst, uint16_t b)
	{
		inst.indirect_flag = false;
		if(!inst.state.fadecount) {
			if(inst.state.secret == 0x81 && !inst.state.demo_flag)
				draw_message(inst, _lvlcomplete2_g, 0xFFFFFF, 0);
			else if(inst.state.secret == 0x82 && !inst.state.demo_flag)
				draw_message(inst, _lvlcomplete3_g, 0xFFFFFF, 0);
			else
				draw_message(inst, _lvlcomplete_g, 0xFFFFFF, 0);
			if(inst.state.timeattack || inst.state.demo_flag)
				draw_timeattack_time(inst, inst.state.waited);
			if(!inst.state.demo_flag && inst.state.stage > 0 && inst.state.sram[inst.state.stage]
				< 255)
				inst.state.sram[inst.state.stage]++;
			if(!inst.state.demo_flag && inst.state.stage > 0 && inst.state.stage < 30)
				inst.state.stage++;
		}
		++inst.state.fadecount;
		//In timeattack mode, only start clears the screen.
		if(inst.state.timeattack)
			return (b & 32) ? state_level_fadeout : state_level_complete;
		if(b & 112)
			return state_level_fadeout;
		return (inst.state.fadecount == 48) ? state_level_fadeout : state_level_complete;
	}

	uint8_t do_menu(struct instance& inst, uint16_t b)
	{
		inst.indirect_flag = false;
		if(inst.state.savestage) {
			inst.state.stage = inst.state.savestage;
			inst.state.savestage = 0;
		}
		if(inst.state.stage == 0)
			inst.state.stage = 1;
		if(inst.state.oldstage != inst.state.stage) {
			if(inst.state.oldstage) {
				//Erase old mark.
				uint16_t txpos = stage_to_xpos(inst.state.oldstage);
				uint16_t typos = stage_to_ypos(inst.state.oldstage);
				render_framebuffer_update(inst, txpos, typos, 45, 7);
			}
			inst.state.oldstage = inst.state.stage;
			uint16_t txpos = stage_to_xpos(inst.state.stage);
			uint16_t typos = stage_to_ypos(inst.state.stage);
			uint32_t color = inst.origbuffer[320 * typos + txpos];
			for(unsigned i = 0; i < FB_SCALE * 7; i++)
				for(unsigned j = 0; j < FB_SCALE  * 45; j++) {
					size_t p = (i + FB_SCALE * typos) * FB_WIDTH + (j + FB_SCALE * txpos);
					if(inst.framebuffer[p] == color)
						inst.framebuffer[p] = 0xFFC080;
				}
		}
		uint16_t rising = b & ~inst.state.lastkeys;
		if(rising & 1)		//Left.
			inst.state.stage = (inst.state.stage > 15) ? (inst.state.stage - 15) :
				(inst.state.stage + 15);
		if(rising & 2)		//Right.
			inst.state.stage = (inst.state.stage > 15) ? (inst.state.stage - 15) :
				(inst.state.stage + 15);
		if(rising & 4)		//Up.
			inst.state.stage = ((inst.state.stage % 15) == 1) ? (inst.state.stage + 14) :
				(inst.state.stage - 1);
		if(rising & 8)		//Down.
			inst.state.stage = ((inst.state.stage % 15) == 0) ? (inst.state.stage - 14) :
				(inst.state.stage + 1);
		if(rising & 48) {	//A or Start.
			if((b & 3) == 3) {
				inst.state.savestage = inst.state.stage;
				inst.state.stage = 0;
			}
			inst.state.demo_flag = ((b & 64) != 0);
			return state_menu_fadeout;
		}
		if(b != inst.state.lastkeys)
			inst.state.waited = 0;
		else
			if(++inst.state.waited == DEMO_WAIT) {
				inst.state.savestage = inst.state.stage;
				inst.state.stage = 0;
				inst.state.demo_flag = 2;
				return state_menu_fadeout;
			}
		return state_menu;
	}

	uint8_t do_load_menu(struct instance& inst, uint16_t b)
	{
		inst.state.cursong = inst.state.rng.pull();
		reload_song(inst, true, inst.state.cursong);
		inst.mplayer.rewind();
		inst.state.oldstage = 0;
		for(unsigned i = 0; i < 64000; i++)
			inst.origbuffer[i] = inst.levelselect[i];
		render_backbuffer(inst);
		for(unsigned i = 1; i < 31; i++)
			for(unsigned j = 0; j < inst.state.sram[i] && j < 7; j++)
				draw_bitmap(inst, complete_mark, 3 * stage_to_xpos(i) + 21 * j + 141,
					3 * stage_to_ypos(i) + 3);
		inst.indirect_flag = true;
		memset(inst.fadeffect_buffer, 0, sizeof(inst.fadeffect_buffer));
		inst.state.waited = 0;
		return state_menu_fadein;
	}

	uint8_t do_load_level(struct instance& inst, uint16_t b)
	{
		inst.state.waited = 0;
		inst.state.secret = 0;
		if(inst.state.state == state_load_level) {
			inst.state.timeattack = (!inst.state.demo_flag && b & 64) ? 1 : 0;
			inst.state.cursong = inst.state.rng.pull();
			reload_song(inst, false, inst.state.cursong);
			inst.mplayer.rewind();
		}
		inst.indirect_flag = true;
		memset(inst.fadeffect_buffer, 0, sizeof(inst.fadeffect_buffer));
		if(!inst.levels.present(inst.state.stage)) {
			return state_level_unavail;
		}
		inst.state.curlevel = level(inst.levels[inst.state.stage]);
		if((inst.state.curlevel.get_o2_amount() * 36) % 65536 == 0 ||
			inst.state.curlevel.get_fuel_amount() == 0)
			return state_lockup;
		inst.state.level_init(inst.state.stage);
		combine_background(inst, inst.state.stage ? (inst.state.stage - 1) / 3 : 0);
		uint8_t hash[32];
		inst.levels[inst.state.stage].sha256_hash(hash);
		if(!memcmp(hash, hash1, 32)) inst.state.secret = 1;
		if(!memcmp(hash, hash2, 32)) inst.state.secret = 2;
		try {
			if(inst.state.demo_flag == 2) {
				inst.state.curdemo = inst.builtin_demo;
			} else if(inst.state.demo_flag) {
				inst.state.curdemo = lookup_demo(inst, hash);
			} else {
				inst.state.curdemo = demo();
			}
		} catch(...) {
			return state_demo_unavail;
		}
		rebuild_pipe_quad_caches(inst, inst.state.curlevel.get_palette_color(68),
			inst.state.curlevel.get_palette_color(69), inst.state.curlevel.get_palette_color(70),
			inst.state.curlevel.get_palette_color(71));
		draw_grav_g_meter(inst);
		draw_level(inst);
		return state_level_fadein;
	}

	uint8_t do_level_play(struct instance& inst, uint16_t b)
	{
		inst.indirect_flag = false;
		//Handle demo.
		b = inst.state.curdemo.fetchkeys(b, inst.state.p.lpos, inst.state.p.framecounter);
		if((b & 96) == 96) {
			inst.state.p.death = physics::death_escaped;
			return state_level_fadeout;
		}
		if((b & ~inst.state.lastkeys) & 32)
			inst.state.paused = inst.state.paused ? 0 : 1;
		if(inst.state.paused)
			return state_level_play;
		int lr = 0, ad = 0;
		bool jump = ((b & 16) != 0);
		if((b & 1) != 0) lr--;
		if((b & 2) != 0) lr++;
		if((b & 4) != 0) ad++;
		if((b & 8) != 0) ad--;
		if((b & 256) != 0) lr = 2;	//Cheat for demo.
		if((b & 512) != 0) ad = 2;	//Cheat for demo.
		uint8_t death = inst.state.simulate_frame(inst.gsfx, lr, ad, jump);
		if(!inst.state.p.death && inst.state.waited < 65535)
			inst.state.waited++;
		draw_level(inst);
		if(inst.state.timeattack)
			draw_timeattack_time(inst, inst.state.waited);
		draw_gauges(inst);
		if(death == physics::death_finished)
			return state_level_complete;
		else if(death)
			return state_level_fadeout_retry;
		else
			return state_level_play;
	}

	uint8_t do_lockup(struct instance& inst, uint16_t b)
	{
		inst.mplayer.set_song(NULL);
		return state_lockup;
	}

	void lstate_lockup(struct instance& inst)
	{
		inst.mplayer.set_song(NULL);
		memset(inst.framebuffer, 0, sizeof(inst.framebuffer));
		memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
	}

	void lstate_demo_unavail(struct instance& inst)
	{
		inst.mplayer.set_song(NULL);
		memset(inst.framebuffer, 0, sizeof(inst.framebuffer));
		memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
		draw_message(inst, _demounavail_g, 0xFFFFFF, 0);
	}

	void lstate_level_unavail(struct instance& inst)
	{
		inst.mplayer.set_song(NULL);
		memset(inst.framebuffer, 0, sizeof(inst.framebuffer));
		memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
		draw_message(inst, _lvlunavail_g, 0xFFFFFF, 0);
	}

	void lstate_level(struct instance& inst)
	{
		reload_song(inst, false, inst.state.cursong);
		inst.mplayer.do_preroll();
		combine_background(inst, inst.state.stage ? (inst.state.stage - 1) / 3 : 0);
		inst.state.speedind = 0;
		inst.state.fuelind = 0;
		inst.state.o2ind = 0;
		inst.state.distind = 0;
		inst.state.lockind = 0;
		level& c = inst.state.curlevel;
		rebuild_pipe_quad_caches(inst, c.get_palette_color(68), c.get_palette_color(69),
			c.get_palette_color(70), c.get_palette_color(71));
		draw_grav_g_meter(inst);
		draw_gauges(inst);
		draw_level(inst);
	}

	void lstate_menu(struct instance& inst)
	{
		reload_song(inst, true, inst.state.cursong);
		inst.mplayer.do_preroll();
		for(unsigned i = 0; i < 64000; i++)
			inst.origbuffer[i] = inst.levelselect[i];
		render_backbuffer(inst);
		for(unsigned i = 1; i < 31; i++)
			for(unsigned j = 0; j < inst.state.sram[i] && j < 7; j++)
				draw_bitmap(inst, complete_mark, FB_SCALE * stage_to_xpos(i) + 21 * j + FB_SCALE * 47,
					FB_SCALE * stage_to_ypos(i) + FB_SCALE);
		if(inst.state.state == state_menu || inst.state.state == state_menu_fadeout) {
			uint16_t txpos = stage_to_xpos(inst.state.stage);
			uint16_t typos = stage_to_ypos(inst.state.stage);
			uint32_t color = inst.origbuffer[320 * typos + txpos];
			for(unsigned i = 0; i < FB_SCALE * 7; i++)
				for(unsigned j = 0; j < FB_SCALE * 45; j++) {
					size_t p = (i + FB_SCALE * typos) * FB_WIDTH + (j + FB_SCALE * txpos);
					if(inst.framebuffer[p] == color)
						inst.framebuffer[p] = 0xFFC080;
				}
		}
	}

	typedef uint8_t (*do_state_t)(struct instance& inst, uint16_t b);
	const do_state_t statefn[] = {
		do_menu_fadein,do_menu,do_menu_fadeout,do_load_level,do_level_fadein,do_level_play,
		do_level_complete, do_level_fadeout,do_load_menu,do_level_unavail,do_demo_unavail,
		do_level_fadeout_retry, do_load_level, do_lockup
	};

	typedef void (*do_lstate_t)(struct instance& inst);
	const do_lstate_t lstatefn[] = {
		lstate_menu,lstate_menu,lstate_menu,NULL,lstate_level,lstate_level,
		lstate_level,lstate_level,NULL,lstate_level_unavail,lstate_demo_unavail,
		lstate_level, lstate_level, lstate_lockup
	};

	bool state_needs_input[] = {
		false,		//Menu fadein
		true,		//Menu needs input for selections.
		false,		//Menu fadeout.
		true,		//Level load needs input for timeattack mode.
		false,		//Level fadein.
		true,		//Level play needs input for actual playing.
		true,		//Level complete needs input to break out before time.
		false,		//Level fadeout.
		false,		//Loading the menu (initial boot vector):
		true,		//Level unavailable needs input to break out of it.
		true,		//Demo unavailable needs input to break out of it.
		false,		//Level fadeout for retry.
		false,		//Load level for retry. Unlike primary load, this does not need input.
		false,		//Lockup.
	};

	void rom_boot_vector(struct instance& inst)
	{
		inst.state.stage = inst.state.savestage = inst.state.oldstage = 0;
		inst.state.change_state(state_load_menu);
	}

	void simulate_frame(struct instance& inst, uint16_t b)
	{
		uint8_t retstate;
		inst.state.rng.push(b);
		inst.state.frames_ran++;
		if(inst.state.state > state_lockup) {
			messages << "Invalid state in simulate: " << (int)inst.state.state << std::endl;
			inst.state.change_state(state_load_menu);	//Reboot.
		}
		retstate = statefn[inst.state.state](inst, b);
		if(retstate != inst.state.state)
			inst.state.change_state(retstate);
		inst.state.lastkeys = b;
	}

	bool simulate_needs_input(struct instance& inst)
	{
		if(inst.state.state > state_lockup)
			return false;
		return state_needs_input[inst.state.state];
	}

	void handle_loadstate(struct instance& inst)
	{
		messages << "Loadstate status: " << (int)inst.state.state << std::endl;
		if(inst.state.state > state_lockup) {
			messages << "Invalid state in loadstate: " << (int)inst.state.state << std::endl;
			inst.state.change_state(state_load_menu);	//Reboot.
		}
		if(lstatefn[inst.state.state])
			lstatefn[inst.state.state](inst);
	}
}
