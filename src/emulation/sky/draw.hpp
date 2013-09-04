#ifndef _skycore__draw__hpp__included__
#define _skycore__draw__hpp__included__

#include "state.hpp"

namespace sky
{
	void draw_grav_g_meter(struct instance& s);
	void draw_gauges(struct instance& s);
	void draw_level(struct instance& inst);
	void rebuild_pipe_quad_caches(struct instance& inst, uint32_t color1, uint32_t color2, uint32_t color3,
		uint32_t color4);
	void draw_timeattack_time(struct instance& inst, uint16_t frames);
}
#endif
