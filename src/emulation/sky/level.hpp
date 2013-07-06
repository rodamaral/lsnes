#ifndef _skycore__level__hpp__included__
#define _skycore__level__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <vector>
#include "tile.hpp"

namespace sky
{
	struct rom_level;

	struct level
	{
		level();
		level(rom_level& data);
		int16_t get_gravity() { return gravity; }
		int16_t get_o2_amount() { return o2_amount; }
		uint16_t get_fuel_amount() { return fuel_amount; }
		uint32_t get_palette_color(size_t idx, bool setC24 = false)
		{
			return (setC24 ? 0x1000000 : 0) | ((idx < 72) ? palette[idx] : 0);
		}
		tile at(uint32_t lpos, uint16_t hpos);
		tile at_tile(int32_t l, int16_t h);
		bool collides(uint32_t lpos, uint16_t hpos, int16_t vpos);
		bool in_pipe(uint32_t lpos, uint16_t hpos, int16_t vpos);
		uint32_t finish_line() { return 65536UL * length - 32768; }
		uint32_t apparent_length() { return 65536UL * (length - 3); }
	private:
		int16_t length;
		int16_t gravity;
		int16_t o2_amount;
		uint16_t fuel_amount;
		uint32_t palette[72];
		uint16_t tiles[32620];
	};

	struct rom_level
	{
		rom_level(const std::vector<char>& data, size_t offset, size_t length);
		int16_t get_gravity() { return gravity; }
		int16_t get_o2_amount() { return o2_amount; }
		uint16_t get_fuel_amount() { return fuel_amount; }
		uint16_t get_length() { return tiles.size() / 7; }
		uint32_t get_palette_color(size_t idx) { return (idx < 72) ? palette[idx] : 0; }
		tile get_tile(size_t idx) { return (idx < tiles.size()) ? tiles[idx] : tile(); }
		void sha256_hash(uint8_t* buffer);
	private:
		int16_t gravity;
		int16_t o2_amount;
		uint16_t fuel_amount;
		uint32_t palette[72];
		std::vector<tile> tiles;
		tile blank;
	};

	struct roads_lzs
	{
		roads_lzs();
		~roads_lzs();
		roads_lzs(const std::vector<char>& file);
		roads_lzs(const roads_lzs& l);
		roads_lzs& operator=(const roads_lzs& l);
		bool present(size_t idx) { return (idx < 31 && l[idx] != NULL); }
		rom_level& operator[](size_t idx) { return *l[idx]; }
	private:
		rom_level* l[31];
	};
}
#endif
