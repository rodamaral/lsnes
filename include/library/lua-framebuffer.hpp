#ifndef _library__lua_framebuffer__hpp__included__
#define _library__lua_framebuffer__hpp__included__

struct lua_render_context
{
	uint32_t left_gap;
	uint32_t right_gap;
	uint32_t top_gap;
	uint32_t bottom_gap;
	struct framebuffer::queue* queue;
	uint32_t width;
	uint32_t height;
};

#endif
