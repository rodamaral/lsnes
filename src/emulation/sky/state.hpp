#ifndef _skycore__state__hpp__included__
#define _skycore__state__hpp__included__

#include <cstdlib>
#include <cstdint>
#include "physics.hpp"
#include "music.hpp"
#include "demo.hpp"
#include "level.hpp"
#include "sound.hpp"
#include "random.hpp"

namespace sky
{
	const uint8_t state_menu_fadein = 0;		//Menu fading in.
	const uint8_t state_menu = 1;			//In menu.
	const uint8_t state_menu_fadeout = 2;		//Menu fading out.
	const uint8_t state_load_level = 3;		//Level being loaded.
	const uint8_t state_level_fadein = 4;		//Level fading in.
	const uint8_t state_level_play = 5;		//Level being played.
	const uint8_t state_level_complete = 6;		//Level completed.
	const uint8_t state_level_fadeout = 7;		//Level fading out.
	const uint8_t state_load_menu = 8;		//Menu being loaded.
	const uint8_t state_level_unavail = 9;		//Level unavailable.
	const uint8_t state_demo_unavail = 10;		//Demo unavailable.
	const uint8_t state_level_fadeout_retry = 11;	//Level fading out for retry.
	const uint8_t state_load_level_nomus = 12;	//Level being loaded, without reloading music.
	const uint8_t state_lockup = 13;		//Game is locked up.

	struct gstate
	{
		//DO NOT PUT POINTERS IN HERE!!!
		//Also, be careful not to do anything undefined if there is a bad value.
		demo curdemo;
		level curlevel;
		active_sfx_dma dma;
		random rng;
		music_player_memory music;
		physics p;
		uint64_t pcmpos;		//PCM position in song.
		uint64_t frames_ran;		//Number of frames run.
		uint32_t cursong;		//Current song number.
		uint16_t waited;		//Menu wait / timeattack time.
		uint8_t paused;			//Paused flag.
		uint8_t speedind;		//Indicated speed.
		uint8_t o2ind;			//Indicated amount of oxygen.
		uint8_t fuelind;		//Indicated amount of fuel.
		uint8_t distind;		//Indicated distance.
		uint8_t lockind;		//Lock indicator flag.
		uint8_t beep_phase;		//Out of O2/Fuel flash phase.
		uint8_t state;			//State of game.
		uint8_t fadecount;		//Fade counter.
		uint8_t stage;			//Current stage.
		uint8_t oldstage;		//old stage (used in menu).
		uint8_t savestage;		//Saved stage (used over demo).
		uint8_t demo_flag;		//Set to 1 to load demo.
		uint8_t lastkeys;		//Last key state.
		uint8_t secret;			//Secret flag.
		uint8_t timeattack;		//Timeattack flag
		uint8_t padC;			//Padding.
		uint8_t padD;			//Padding.
		uint8_t sram[32];		//SRAM.
		void level_init(uint8_t _stage);
		uint8_t simulate_frame(noise_maker& sfx, int lr, int ad, bool jump);
		void change_state(uint8_t newstate);
		std::pair<uint8_t*, size_t> as_ram();
	};
}

#endif
