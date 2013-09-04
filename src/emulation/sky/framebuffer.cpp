#include "framebuffer.hpp"
#include "instance.hpp"
#include <cstring>
#include <iostream>

namespace sky
{
	void render_backbuffer(struct instance& inst)
	{

		for(unsigned i = 0; i < 200; i++) {
			for(unsigned j = 0; j < 320; j++) {
				for(unsigned k = 0; k < FB_SCALE; k++)
					inst.framebuffer[FB_MAJSTRIDE * i + FB_SCALE * j + k] = inst.origbuffer[
						320 * i + j];
			}
			for(unsigned k = 1; k < FB_SCALE; k++)
				memcpy(inst.framebuffer + (FB_MAJSTRIDE * i + k * FB_WIDTH),
					inst.framebuffer + FB_MAJSTRIDE * i, FB_WIDTH * sizeof(uint32_t));
		}
		//Calculate overlap region.
		for(unsigned i = 0; i < 200; i++) {
			uint32_t high = 0x00000000U;
			for(unsigned j = 0; j < 320; j++)
				high |= inst.origbuffer[320 * i + j];
			if(high & 0xFF000000U) {
				inst.overlap_start = FB_SCALE * i;
				break;
			}
		}
		for(unsigned i = inst.overlap_start / FB_SCALE; i < 200; i++) {
			uint32_t low = 0xFFFFFFFFU;
			for(unsigned j = 0; j < 320; j++)
				low &= inst.origbuffer[320 * i + j];
			if((low & 0xFF000000U) == 0xFF000000U) {
				inst.overlap_end = FB_SCALE * i;
				break;
			}
		}
	}

	void render_framebuffer_update(struct instance& inst, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
	{
		if(x >= 320)
			return;
		for(unsigned i = y; i < y + h && i < 200; i++) {
			for(unsigned j = x; j < x + w && j < 320; j++) {
				for(unsigned k = 0; k < FB_SCALE; k++)
					inst.framebuffer[FB_MAJSTRIDE * i + FB_SCALE * j + k] = inst.origbuffer[
						320 * i + j];
			}
			for(unsigned k = 1; k < FB_SCALE; k++)
				memcpy(inst.framebuffer + (FB_MAJSTRIDE * i + FB_SCALE * x + k * FB_WIDTH),
					inst.framebuffer + FB_MAJSTRIDE * i + FB_SCALE * x, FB_SCALE * w *
					sizeof(uint32_t));
		}
	}

	void render_framebuffer_vline(struct instance& inst, uint16_t x, uint16_t y1, uint16_t y2, uint32_t color)
	{
		for(unsigned i = y1; i <= y2 && i < 200; i++) {
			for(unsigned k = 0; k < FB_SCALE; k++)
				inst.framebuffer[FB_MAJSTRIDE * i + x + k * FB_WIDTH] = 0xFF000000U | color;
		}
	}

	char decode(const char* ch)
	{
		char c = *ch;
		if(c >= '\\')
			c--;
		return c - 35;
	}

	void draw_block2(struct instance& inst, const char* pointer, uint16_t position, uint32_t c1, uint32_t c2,
		bool zero)
	{
		static const uint8_t tbl[3][6] = {{27, 9, 3, 1}, {32, 16, 8, 4, 2, 1}, {32, 16, 8, 4, 2, 1}};
		static const uint8_t tbl2[3][3] = {{4, 6, 6}, {3, 2, 2}, {0, 0, 1}};
		c1 |= 0xFF000000U;
		c2 |= 0xFF000000U;
		uint8_t type = decode(pointer++);
		uint16_t width = decode(pointer++);
		if(width > 63) width = (width - 64) * 64 + decode(pointer++);
		uint16_t height = decode(pointer++);
		if(height > 63) height = (height - 64) * 64 + decode(pointer++);
		uint16_t origptr = position;
		uint8_t mod = 0;
		uint8_t reg = 0;
		for(unsigned y = 0; y < height; y++) {
			for(unsigned x = 0; x < width; x++) {
				if(!mod)
					reg = decode(pointer++);
				uint8_t p = ((reg / tbl[type][mod]) % tbl2[1][type]) << tbl2[2][type];
				mod = (mod + 1) % tbl2[0][type];
				if(p)
					inst.origbuffer[position] = (p > 1) ? c2 : c1;
				else if(zero)
					inst.origbuffer[position] = 0;
				position++;
			}
			position += (320 - width);
		}
		render_framebuffer_update(inst, origptr % 320, origptr / 320, width, height);
	}

	void draw_block(struct instance& inst, const uint8_t* pointer, uint16_t position, uint32_t c1, uint32_t c2,
		bool zero)
	{
		c1 |= 0xFF000000U;
		c2 |= 0xFF000000U;
		uint16_t width = *(pointer++);
		uint16_t height = *(pointer++);
		uint16_t origptr = position;
		for(unsigned y = 0; y < height; y++) {
			for(unsigned x = 0; x < width; x++) {
				uint8_t p = *(pointer++);
				if(p)
					inst.origbuffer[position] = (p > 1) ? c2 : c1;
				else if(zero)
					inst.origbuffer[position] = 0;
				position++;
			}
			position += (320 - width);
		}
		render_framebuffer_update(inst, origptr % 320, origptr / 320, width, height);
	}

	void draw_message(struct instance& inst, const char* pointer, uint32_t c1, uint32_t c2)
	{
		auto op = pointer;
		pointer++;
		uint16_t width = decode(pointer++);
		if(width > 63) width = (width - 64) * 64 + decode(pointer++);
		uint16_t height = decode(pointer++);
		if(height > 63) height = (height - 64) * 64 + decode(pointer++);
		uint16_t x = (320 - width) / 2;
		uint16_t y = (200 - height) / 2;
		uint16_t p = 320 * y + x;
		draw_block2(inst, op, p, c1, c2);
	}

	void draw_distance_column(struct instance& inst, uint16_t col, uint32_t c)
	{
		uint16_t minline = 0x8f;
		uint16_t maxline = 0x8f;
		uint16_t ptr = 320 * 0x8f + 0x2a + (col / FB_SCALE);
		uint32_t px = inst.origbuffer[ptr] ;
		while(inst.origbuffer[ptr] == px)
			ptr -= 320;
		ptr += 320;
		minline = ptr / 320;
		while(inst.origbuffer[ptr] == px)
			ptr += 320;
		maxline = ptr / 320 - 1;
		render_framebuffer_vline(inst, col + FB_SCALE * 0x2a, minline, maxline, c | 0xFF000000U);
	}

	void blink_between(struct instance& inst, unsigned x, unsigned y, unsigned w, unsigned h, uint32_t c1,
		uint32_t c2)
	{
		c1 &= 0xFFFFFF;
		c2 &= 0xFFFFFF;
		uint32_t c1x = c1 | 0xFF000000U;
		uint32_t c2x = c2 | 0xFF000000U;
		for(unsigned j = y; j < y + h; j++)
			for(unsigned i = x; i < x + w; i++) {
				if((inst.origbuffer[320 * j + i] & 0xFFFFFF) == c1)
					inst.origbuffer[320 * j + i] = c2x;
				else if((inst.origbuffer[320 * j + i] & 0xFFFFFF) == c2)
					inst.origbuffer[320 * j + i] = c1x;
			}
		render_framebuffer_update(inst, x, y, w, h);
	}

	void draw_bitmap(struct instance& inst, const uint32_t* bitmap, unsigned x, unsigned y)
	{
		for(unsigned j = 0; j < bitmap[1]; j++)
			for(unsigned i = 0; i < bitmap[0]; i++)
				inst.framebuffer[(y + j) * FB_WIDTH + (x + i)] = bitmap[j * bitmap[0] + i + 2];
	}
}
