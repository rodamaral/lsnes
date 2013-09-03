#include "tile.hpp"

namespace sky
{
	const uint16_t _floorheights[6] = {0, 12800, 15360, 17000, 20000, 25000 };

	const uint8_t _floor_height[256] = {
		0, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5
	};

	const uint8_t _tunnel_inner[38] = {
		16,	16,	16,	16,	15,	14,	13,	11,
		 8,	 7,	 6,	 5,	 3,	 3,	 3,	 3,
		 3,	 3,	 2,	 1,	 0,	 0,	 0,	 0,
		 0,	 0,	 1,	 2,	 3,	 3,	 3,	 3,
		 3,	 3,	 5,	 6,	 7,	 8
	};

	const uint8_t _tunnel_outer[38] = {
		32,	32,	32,	32,	32,	32,	32,	32,
		32,	32,	32,	32,	32,	32,	32,	32,
		32,	31,	31,	31,	31,	31,	30,	30,
		30,	29,	29,	29,	28,	27,	26,	25,
		24,	22,	20,	18,	17,	14
	};

	int tile::apparent_height() throw()
	{
		if((_tile & 0xE00) <= 0x400)
			return (_tile & 0xF00) >> 9;
		else
			return -1;
	}
	bool tile::is_colliding_floor(int16_t hchunk, int16_t vpos)
	{
		return has_lower_floor() && vpos > 7808 && vpos < 10240;
	}
	bool tile::is_colliding(int16_t hchunk, int16_t vpos)
	{
		if(hchunk <= 0)
			hchunk = 1 - hchunk;
		if(vpos <= 8576 || hchunk > 37)
			return false;
		//Glitched tiles
		if((_tile & 0xE00) >= 0x600)
			return true;
		//Tunnels.
		if((_tile & 0x100) != 0) {
			int16_t rvpos = vpos - 8704;
			if(rvpos >= 0 && rvpos < (128 * _tunnel_inner[hchunk]))
				return false;
		}
		//Upper limits.
		if((_tile & 0xE00) == 0x400 && vpos >= 15360)
			return false;
		if((_tile & 0xE00) == 0x200 && vpos >= 12800)
			return false;
		if((_tile & 0xF00) == 0 && vpos >= 10240)
			return false;
		//The bare turnnel special.
		if((_tile & 0xF00) == 0x100) {
			int16_t rvpos = vpos - 8704;
			if(rvpos >= 0 && rvpos >= (128 * _tunnel_outer[hchunk]))
				return false;
		}
		return true;
	}
	int16_t tile::upper_level()
	{
		return _floorheights[_floor_height[_tile >> 8]];
	}
	unsigned tile::surface_type(int16_t vpos)
	{
		if(vpos == 10240)
			return lower_floor();
		else if(vpos == upper_level())
			return upper_floor();
		else
			return 16;	//None.
	}
	bool tile::is_dangerous()
		{
		if((_tile & 0xF00) == 0)
			return ((_tile & 0xF) == 0x00 || (_tile & 0xF) == 0xC);
		if((_tile & 0xF00) == 0x100)
			return false;
		return ((_tile & 0xF0) == 0xC0);
	}
	bool tile::in_pipe(uint16_t hchunk, int16_t vpos)
	{
		if(hchunk > 23 || !is_tunnel())
			return false;
		return (vpos - 8704 < ((_tunnel_inner[hchunk] + _tunnel_outer[hchunk]) / 2) * 128);
	}
}
