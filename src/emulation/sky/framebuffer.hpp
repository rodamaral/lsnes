#ifndef _skycore__framebuffer__hpp__included__
#define _skycore__framebuffer__hpp__included__

#include <cstdint>

#define FB_SCALE 3
#define FB_WIDTH (FB_SCALE * 320)
#define FB_HEIGHT (FB_SCALE * 200)
#define FB_MAJSTRIDE (FB_SCALE * FB_SCALE * 320)

namespace sky
{
	extern uint32_t origbuffer[65536];
	extern uint32_t framebuffer[FB_WIDTH * FB_HEIGHT];
	extern uint16_t overlap_start;
	extern uint16_t overlap_end;

	inline void framebuffer_blend1(uint32_t& fbpix, uint32_t rpixel)
	{
		fbpix = rpixel & 0x00FFFFFFU;
	}

	inline void framebuffer_blend2(uint32_t& fbpix, uint32_t rpixel)
	{
		if((fbpix >> 31) == 0)
			fbpix = rpixel & 0x01FFFFFFU;
	}

	//Take origbuffer and completely rerender framebuffer.
	void render_backbuffer();
	//Take a reigon of origbuffer and rerender that into framebuffer.
	void render_framebuffer_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
	//Render a vertical line into framebuffer. Uses n*320x200 screen size!
	void render_framebuffer_vline(uint16_t x, uint16_t y1, uint16_t y2, uint32_t color);
	//Render a gauge block.
	void draw_block(const uint8_t* pointer, uint16_t position, uint32_t c1, uint32_t c2, bool zero = false);
	void draw_block2(const char* pointer, uint16_t position, uint32_t c1, uint32_t c2, bool zero = false);
	//Render a message.
	void draw_message(const char* pointer, uint32_t c1, uint32_t c2);
	//Draw column in distance gauge.
	void draw_distance_column(uint16_t col, uint32_t c);
	//Blink between colors in specified block.
	void blink_between(unsigned x, unsigned y, unsigned w, unsigned h, uint32_t c1, uint32_t c2);
	//Draw a bitmap at full resolution.
	void draw_bitmap(const uint32_t* bitmap, unsigned x, unsigned y);
}

#endif
