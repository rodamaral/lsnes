#ifndef _skycore__romimage__hpp__included__
#define _skycore__romimage__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "gauge.hpp"
#include "level.hpp"
#include "image.hpp"
#include "sound.hpp"
#include "demo.hpp"

namespace sky
{
	extern std::string rom_filename;
	extern gauge speed_dat;
	extern gauge oxydisp_dat;
	extern gauge fueldisp_dat;
	extern roads_lzs levels;
	extern image ship;
	extern image dashboard;
	extern image levelselect;
	extern image backgrounds[10];
	extern sounds soundfx;
	extern demo builtin_demo;
	extern uint32_t dashpalette[16];
	void load_rom(const std::string& filename);
	void combine_background(size_t back);
	demo lookup_demo(const uint8_t* levelhash);
}


#endif