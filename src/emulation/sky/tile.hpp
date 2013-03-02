#ifndef _skycore__tile__hpp__included__
#define _skycore__tile__hpp__included__

#include <cstdint>

namespace sky
{
	struct tile
	{
		static const unsigned sticky = 2;
		static const unsigned slippery = 8;
		static const unsigned suppiles = 9;
		static const unsigned boost = 10;
		static const unsigned burning = 12;
		tile() { _tile = 0; }
		tile(uint16_t xtile) { _tile = xtile; }
		unsigned lower_floor() { return _tile & 0xF; }
		unsigned upper_floor() { return (_tile & 0xF0) >> 4; }
		bool is_tunnel() { return ((_tile & 0x100) != 0) && ((_tile & 0xE00) <= 0x400); }
		int apparent_height() throw();
		bool has_lower_floor() throw() { return ((_tile & 0xF) != 0); }
		bool is_colliding_floor(int16_t hchunk, int16_t vpos);
		bool is_colliding(int16_t hchunk, int16_t vpos);
		int16_t upper_level();
		unsigned surface_type(int16_t vpos);
		bool is_dangerous();
		bool is_block() { return ((_tile & 0xF00) != 0); }
		bool is_rblock() { return ((_tile & 0xE00) != 0); }
		bool is_blank() { return (_tile == 0); /* Not masked! */ }
		bool in_pipe(uint16_t hchunk, int16_t vpos);
		uint16_t rawtile() { return _tile; }
	private:
		uint16_t _tile;
	};
}
#endif
