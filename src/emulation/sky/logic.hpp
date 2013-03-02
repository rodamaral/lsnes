#ifndef _skycore__logic__hpp__included__
#define _skycore__logic__hpp__included__

#include <cstdlib>
#include <cstdint>
#include "state.hpp"
#include "framebuffer.hpp"

namespace sky
{
	void simulate_frame(gstate& s, uint16_t b);
	void rom_boot_vector(gstate& s);
	void handle_loadstate(gstate& s);
	extern uint32_t fadeffect_buffer[FB_WIDTH * FB_HEIGHT];
	extern bool indirect_flag;
}
#endif