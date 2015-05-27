#ifndef _sky__instance__hpp__included__
#define _sky__instance__hpp__included__

#include "state.hpp"
#include "gauge.hpp"
#include "image.hpp"
#include "framebuffer.hpp"

namespace sky
{
	const unsigned pipe_slices = 256;
	struct pipe_cache
	{
		double min_h;
		double min_v;
		double max_h;
		double max_v;
		uint32_t colors[pipe_slices];
	};

	struct demoset_entry
	{
		uint8_t hash[32];
		std::vector<char> demodata;
	};

	struct instance
	{
		instance()
			: gsfx(soundfx, state.dma),
			mplayer(state.music, state.rng)
		{
			memset(samplectr, 0, sizeof(samplectr));
		}
		gstate state;
		song_buffer* bsong;
		sound_noise_maker gsfx;
		std::string rom_filename;
		gauge speed_dat;
		gauge oxydisp_dat;
		gauge fueldisp_dat;
		roads_lzs levels;
		image ship;
		image dashboard;
		image levelselect;
		image backgrounds[10];
		sounds soundfx;
		demo builtin_demo;
		uint32_t dashpalette[16];
		std::vector<demoset_entry> demos;
		music_player mplayer;
		struct pipe_cache pipecache[7];
		uint32_t fadeffect_buffer[FB_WIDTH * FB_HEIGHT];
		bool indirect_flag;
		uint32_t origbuffer[65536];
		uint32_t framebuffer[FB_WIDTH * FB_HEIGHT];
		uint16_t overlap_start;
		uint16_t overlap_end;
		uint32_t samplectr[4];
		uint32_t* get_framebuffer()
		{
			return indirect_flag ? fadeffect_buffer : framebuffer;
		}
		size_t extrasamples()
		{
			const static unsigned tcount[4] = {5, 7, 8, 25};
			size_t extrasample = 0;
			for(unsigned i = 0; i < sizeof(tcount)/sizeof(tcount[0]); i++) {
				samplectr[i]++;
				if(samplectr[i] == tcount[i]) {
					samplectr[i] = 0;
					extrasample = extrasample ? 0 : 1;
				} else
					break;
			}
			return extrasample;
		}
	};
}

#endif
