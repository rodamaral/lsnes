#include "logic.hpp"
#include "framebuffer.hpp"
#include "music.hpp"
#include "messages.hpp"
#include "romimage.hpp"
#include "draw.hpp"
#include "core/window.hpp"
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
	uint32_t fadeffect_buffer[FB_WIDTH * FB_HEIGHT];
	bool indirect_flag;

	void reload_song(bool menu, uint32_t num)
	{
		if(menu) {
			mplayer.set_song(NULL);
			if(bsong)
				delete bsong;
			bsong = NULL;
			std::string filename = rom_filename + "/menu.opus";
			std::istream* s = NULL;
			try {
				s = &open_file_relative(filename, "");
				messages << "Selected song: " << filename << std::endl;
				bsong = new song_buffer(*s);
				mplayer.set_song(bsong);
				delete s;
			} catch(std::exception& e) {
				if(s) {
					messages << "Unable to load song: " << e.what() << std::endl;
					delete s;
				}
			}
		} else {
			mplayer.set_song(NULL);
			if(bsong)
				delete bsong;
			bsong = NULL;
			std::istream* s = NULL;
			try {
				zip_reader r(rom_filename);
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
				bsong = new song_buffer(*s);
				mplayer.set_song(bsong);
				delete s;
			} catch(std::exception& e) {
				messages << "Unable to load song: " << e.what() << std::endl;
				if(s)
					delete s;
			}
		}
	}

	void faded_framebuffer(uint16_t alpha)
	{
		indirect_flag = true;
		for(unsigned i = 0; i < sizeof(framebuffer) / sizeof(framebuffer[0]); i++) {
			uint32_t L = (((framebuffer[i] & 0x00FF00FF) * alpha) >> 8) & 0x00FF00FF;
			uint32_t H = (((framebuffer[i] & 0xFF00FF00U) >> 8) * alpha) & 0xFF00FF00U;
			fadeffect_buffer[i] = L | H;
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

	uint8_t do_menu_fadein(gstate& s, uint16_t b)
	{
		faded_framebuffer(8 * s.fadecount);
		return (++s.fadecount == 32) ? state_menu : state_menu_fadein;
	}

	uint8_t do_menu_fadeout(gstate& s, uint16_t b)
	{
		faded_framebuffer(256 - 8 * s.fadecount);
		return (++s.fadecount == 32) ? state_load_level : state_menu_fadeout;
	}

	uint8_t do_level_fadein(gstate& s, uint16_t b)
	{
		uint8_t fadelimit = s.timeattack ? 144 : 32;
		faded_framebuffer(256 * s.fadecount / fadelimit);
		return (++s.fadecount == fadelimit) ? state_level_play : state_level_fadein;
	}

	uint8_t do_level_fadeout(gstate& s, uint16_t b)
	{
		faded_framebuffer(256 - 8 * s.fadecount);
		return (++s.fadecount == 32) ? state_load_menu : state_level_fadeout;
	}

	uint8_t do_level_fadeout_retry(gstate& s, uint16_t b)
	{
		faded_framebuffer(256 - 8 * s.fadecount);
		return (++s.fadecount == 32) ? state_load_level_nomus : state_level_fadeout_retry;
	}

	uint8_t do_level_unavail(gstate& s, uint16_t b)
	{
		indirect_flag = false;
		if(!s.fadecount) {
			memset(framebuffer, 0, sizeof(framebuffer));
			memset(origbuffer, 0, sizeof(origbuffer));
			draw_message(_lvlunavail_g, 0xFFFFFF, 0);
		}
		s.fadecount = 1;
		return ((b & ~s.lastkeys) & 0x30) ? state_load_menu : state_level_unavail;
	}

	uint8_t do_demo_unavail(gstate& s, uint16_t b)
	{
		indirect_flag = false;
		if(!s.fadecount) {
			memset(framebuffer, 0, sizeof(framebuffer));
			memset(origbuffer, 0, sizeof(origbuffer));
			draw_message(_demounavail_g, 0xFFFFFF, 0);
		}
		s.fadecount = 1;
		return ((b & ~s.lastkeys) & 0x30) ? state_load_menu : state_demo_unavail;
	}

	uint8_t do_level_complete(gstate& s, uint16_t b)
	{
		indirect_flag = false;
		if(!s.fadecount) {
			if(s.secret == 0x81 && !s.demo_flag)
				draw_message(_lvlcomplete2_g, 0xFFFFFF, 0);
			else if(s.secret == 0x82 && !s.demo_flag)
				draw_message(_lvlcomplete3_g, 0xFFFFFF, 0);
			else
				draw_message(_lvlcomplete_g, 0xFFFFFF, 0);
			if(s.timeattack || s.demo_flag)
				draw_timeattack_time(s.waited);
			if(!s.demo_flag && s.stage > 0 && s.sram[s.stage] < 255)
				s.sram[s.stage]++;
			if(!s.demo_flag && s.stage > 0 && s.stage < 30)
				s.stage++;
		}
		++s.fadecount;
		//In timeattack mode, only start clears the screen.
		if(s.timeattack)
			return (b & 32) ? state_level_fadeout : state_level_complete;
		if(b & 112)
			return state_level_fadeout;
		return (s.fadecount == 48) ? state_level_fadeout : state_level_complete;
	}

	uint8_t do_menu(gstate& s, uint16_t b)
	{
		indirect_flag = false;
		if(s.savestage) {
			s.stage = s.savestage;
			s.savestage = 0;
		}
		if(s.stage == 0)
			s.stage = 1;
		if(s.oldstage != s.stage) {
			if(s.oldstage) {
				//Erase old mark.
				uint16_t txpos = stage_to_xpos(s.oldstage);
				uint16_t typos = stage_to_ypos(s.oldstage);
				render_framebuffer_update(txpos, typos, 45, 7);
			}
			s.oldstage = s.stage;
			uint16_t txpos = stage_to_xpos(s.stage);
			uint16_t typos = stage_to_ypos(s.stage);
			uint32_t color = origbuffer[320 * typos + txpos];
			for(unsigned i = 0; i < FB_SCALE * 7; i++)
				for(unsigned j = 0; j < FB_SCALE  * 45; j++) {
					size_t p = (i + FB_SCALE * typos) * FB_WIDTH + (j + FB_SCALE * txpos);
					if(framebuffer[p] == color)
						framebuffer[p] = 0xFFC080;
				}
		}
		uint16_t rising = b & ~s.lastkeys;
		if(rising & 1)		//Left.
			s.stage = (s.stage > 15) ? (s.stage - 15) : (s.stage + 15);
		if(rising & 2)		//Right.
			s.stage = (s.stage > 15) ? (s.stage - 15) : (s.stage + 15);
		if(rising & 4)		//Up.
			s.stage = ((s.stage % 15) == 1) ? (s.stage + 14) : (s.stage - 1);
		if(rising & 8)		//Down.
			s.stage = ((s.stage % 15) == 0) ? (s.stage - 14) : (s.stage + 1);
		if(rising & 48) {	//A or Start.
			if((b & 3) == 3) {
				s.savestage = s.stage;
				s.stage = 0;
			}
			s.demo_flag = ((b & 64) != 0);
			return state_menu_fadeout;
		}
		if(b != s.lastkeys)
			s.waited = 0;
		else
			if(++s.waited == DEMO_WAIT) {
				s.savestage = s.stage;
				s.stage = 0;
				s.demo_flag = 2;
				return state_menu_fadeout;
			}
		return state_menu;
	}

	uint8_t do_load_menu(gstate& s, uint16_t b)
	{
		s.cursong = s.rng.pull();
		reload_song(true, s.cursong);
		mplayer.rewind();
		s.oldstage = 0;
		for(unsigned i = 0; i < 64000; i++)
			origbuffer[i] = levelselect[i];
		render_backbuffer();
		for(unsigned i = 1; i < 31; i++)
			for(unsigned j = 0; j < s.sram[i] && j < 7; j++)
				draw_bitmap(complete_mark, 3 * stage_to_xpos(i) + 21 * j + 141,
					3 * stage_to_ypos(i) + 3);
		indirect_flag = true;
		memset(fadeffect_buffer, 0, sizeof(fadeffect_buffer));
		s.waited = 0;
		return state_menu_fadein;
	}

	uint8_t do_load_level(gstate& s, uint16_t b)
	{
		s.waited = 0;
		s.secret = 0;
		if(s.state == state_load_level) {
			s.timeattack = (!s.demo_flag && b & 64) ? 1 : 0;
			s.cursong = s.rng.pull();
			reload_song(false, s.cursong);
			mplayer.rewind();
		}
		indirect_flag = true;
		memset(fadeffect_buffer, 0, sizeof(fadeffect_buffer));
		if(!levels.present(s.stage)) {
			return state_level_unavail;
		}
		s.curlevel = level(levels[s.stage]);
		if((s.curlevel.get_o2_amount() * 36) % 65536 == 0 || s.curlevel.get_fuel_amount() == 0)
			return state_lockup;
		s.level_init(s.stage);
		combine_background(s.stage ? (s.stage - 1) / 3 : 0);
		uint8_t hash[32];
		levels[s.stage].sha256_hash(hash);
		if(!memcmp(hash, hash1, 32)) s.secret = 1;
		if(!memcmp(hash, hash2, 32)) s.secret = 2;
		try {
			if(s.demo_flag == 2) {
				s.curdemo = builtin_demo;
			} else if(s.demo_flag) {
				s.curdemo = lookup_demo(hash);
			} else {
				s.curdemo = demo();
			}
		} catch(...) {
			return state_demo_unavail;
		}
		rebuild_pipe_quad_caches(s.curlevel.get_palette_color(68), s.curlevel.get_palette_color(69),
			s.curlevel.get_palette_color(70), s.curlevel.get_palette_color(71));
		draw_grav_g_meter(s);
		draw_level(s);
		return state_level_fadein;
	}

	uint8_t do_level_play(gstate& s, uint16_t b)
	{
		indirect_flag = false;
		//Handle demo.
		b = s.curdemo.fetchkeys(b, s.p.lpos, s.p.framecounter);
		if((b & 96) == 96) {
			s.p.death = physics::death_escaped;
			return state_level_fadeout;
		}
		if((b & ~s.lastkeys) & 32)
			s.paused = s.paused ? 0 : 1;
		if(s.paused)
			return state_level_play;
		int lr = 0, ad = 0;
		bool jump = ((b & 16) != 0);
		if((b & 1) != 0) lr--;
		if((b & 2) != 0) lr++;
		if((b & 4) != 0) ad++;
		if((b & 8) != 0) ad--;
		if((b & 256) != 0) lr = 2;	//Cheat for demo.
		if((b & 512) != 0) ad = 2;	//Cheat for demo.
		uint8_t death = s.simulate_frame(lr, ad, jump);
		if(!s.p.death && s.waited < 65535)
			s.waited++;
		draw_level(s);
		if(s.timeattack)
			draw_timeattack_time(s.waited);
		draw_gauges(s);
		if(death == physics::death_finished)
			return state_level_complete;
		else if(death)
			return state_level_fadeout_retry;
		else
			return state_level_play;
	}

	uint8_t do_lockup(gstate& s, uint16_t b)
	{
		mplayer.set_song(NULL);
		return state_lockup;
	}

	void lstate_lockup(gstate& s)
	{
		mplayer.set_song(NULL);
		memset(framebuffer, 0, sizeof(framebuffer));
		memset(origbuffer, 0, sizeof(origbuffer));
	}

	void lstate_demo_unavail(gstate& s)
	{
		mplayer.set_song(NULL);
		memset(framebuffer, 0, sizeof(framebuffer));
		memset(origbuffer, 0, sizeof(origbuffer));
		draw_message(_demounavail_g, 0xFFFFFF, 0);
	}

	void lstate_level_unavail(gstate& s)
	{
		mplayer.set_song(NULL);
		memset(framebuffer, 0, sizeof(framebuffer));
		memset(origbuffer, 0, sizeof(origbuffer));
		draw_message(_lvlunavail_g, 0xFFFFFF, 0);
	}

	void lstate_level(gstate& s)
	{
		reload_song(false, s.cursong);
		mplayer.do_preroll();
		combine_background(s.stage ? (s.stage - 1) / 3 : 0);
		s.speedind = 0;
		s.fuelind = 0;
		s.o2ind = 0;
		s.distind = 0;
		s.lockind = 0;
		level& c = s.curlevel;
		rebuild_pipe_quad_caches(c.get_palette_color(68), c.get_palette_color(69), c.get_palette_color(70),
			c.get_palette_color(71));
		draw_grav_g_meter(s);
		draw_gauges(s);
		draw_level(s);
	}

	void lstate_menu(gstate& s)
	{
		reload_song(true, s.cursong);
		mplayer.do_preroll();
		for(unsigned i = 0; i < 64000; i++)
			origbuffer[i] = levelselect[i];
		render_backbuffer();
		for(unsigned i = 1; i < 31; i++)
			for(unsigned j = 0; j < s.sram[i] && j < 7; j++)
				draw_bitmap(complete_mark, FB_SCALE * stage_to_xpos(i) + 21 * j + FB_SCALE * 47,
					FB_SCALE * stage_to_ypos(i) + FB_SCALE);
		if(s.state == state_menu || s.state == state_menu_fadeout) {
			uint16_t txpos = stage_to_xpos(s.stage);
			uint16_t typos = stage_to_ypos(s.stage);
			uint32_t color = origbuffer[320 * typos + txpos];
			for(unsigned i = 0; i < FB_SCALE * 7; i++)
				for(unsigned j = 0; j < FB_SCALE * 45; j++) {
					size_t p = (i + FB_SCALE * typos) * FB_WIDTH + (j + FB_SCALE * txpos);
					if(framebuffer[p] == color)
						framebuffer[p] = 0xFFC080;
				}
		}
	}

	typedef uint8_t (*do_state_t)(gstate& s, uint16_t b);
	do_state_t statefn[] = {
		do_menu_fadein,do_menu,do_menu_fadeout,do_load_level,do_level_fadein,do_level_play,
		do_level_complete, do_level_fadeout,do_load_menu,do_level_unavail,do_demo_unavail,
		do_level_fadeout_retry, do_load_level, do_lockup
	};

	typedef void (*do_lstate_t)(gstate& s);
	do_lstate_t lstatefn[] = {
		lstate_menu,lstate_menu,lstate_menu,NULL,lstate_level,lstate_level,
		lstate_level,lstate_level,NULL,lstate_level_unavail,lstate_demo_unavail,
		lstate_level, lstate_level, lstate_lockup
	};

	void rom_boot_vector(gstate& s)
	{
		s.stage = s.savestage = s.oldstage = 0;
		s.change_state(state_load_menu);
	}

	void simulate_frame(gstate& s, uint16_t b)
	{
		uint8_t retstate;
		s.rng.push(b);
		s.frames_ran++;
		if(s.state > state_lockup) {
			messages << "Invalid state in simulate: " << (int)s.state << std::endl;
			s.change_state(state_load_menu);	//Reboot.
		}
		retstate = statefn[s.state](s, b);
		if(retstate != s.state)
			s.change_state(retstate);
		s.lastkeys = b;
	}

	void handle_loadstate(gstate& s)
	{
		messages << "Loadstate status: " << (int)s.state << std::endl;
		if(s.state > state_lockup) {
			messages << "Invalid state in loadstate: " << (int)s.state << std::endl;
			s.change_state(state_load_menu);	//Reboot.
		}
		if(lstatefn[s.state])
			lstatefn[s.state](s);
	}
}
