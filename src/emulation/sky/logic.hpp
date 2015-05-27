#ifndef _skycore__logic__hpp__included__
#define _skycore__logic__hpp__included__

#include <cstdlib>
#include <cstdint>
#include "state.hpp"
#include "framebuffer.hpp"

namespace sky
{
	void simulate_frame(struct instance& inst, uint16_t b);
	bool simulate_needs_input(struct instance& inst);
	void rom_boot_vector(struct instance& inst);
	void handle_loadstate(struct instance& inst);
}
#endif
