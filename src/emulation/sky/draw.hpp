#ifndef _skycore__draw__hpp__included__
#define _skycore__draw__hpp__included__

#include "state.hpp"

namespace sky
{
	void draw_grav_g_meter(gstate& s);
	void draw_gauges(gstate& s);
	void draw_level(gstate& s);
	void rebuild_pipe_quad_caches(uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4);
}
#endif
