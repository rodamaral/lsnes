#include "framebuffer.hpp"
#include <cstring>
#include <iostream>

namespace sky
{
	uint32_t origbuffer[65536];
	uint32_t framebuffer[FB_WIDTH * FB_HEIGHT];
	uint16_t overlap_start;
	uint16_t overlap_end;

	void render_backbuffer()
	{
		
		for(unsigned i = 0; i < 200; i++) {
			for(unsigned j = 0; j < 320; j++) {
				for(unsigned k = 0; k < FB_SCALE; k++)
					framebuffer[FB_MAJSTRIDE * i + FB_SCALE * j + k] = origbuffer[320 * i + j];
			}
			for(unsigned k = 1; k < FB_SCALE; k++)
				memcpy(framebuffer + (FB_MAJSTRIDE * i + k * FB_WIDTH),
					framebuffer + FB_MAJSTRIDE * i, FB_WIDTH * sizeof(uint32_t));
		}
		//Calculate overlap region.
		for(unsigned i = 0; i < 200; i++) {
			uint32_t high = 0x00000000U;
			for(unsigned j = 0; j < 320; j++)
				high |= origbuffer[320 * i + j];
			if(high & 0xFF000000U) {
				overlap_start = FB_SCALE * i;
				break;
			}
		}
		for(unsigned i = overlap_start / FB_SCALE; i < 200; i++) {
			uint32_t low = 0xFFFFFFFFU;
			for(unsigned j = 0; j < 320; j++)
				low &= origbuffer[320 * i + j];
			if((low & 0xFF000000U) == 0xFF000000U) {
				overlap_end = FB_SCALE * i;
				break;
			}
		}
	}

	void render_framebuffer_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
	{
		if(x >= 320)
			return;
		for(unsigned i = y; i < y + h && i < 200; i++) {
			for(unsigned j = x; j < x + w && j < 320; j++) {
				for(unsigned k = 0; k < FB_SCALE; k++)
					framebuffer[FB_MAJSTRIDE * i + FB_SCALE * j + k] = origbuffer[320 * i + j];
			}
			for(unsigned k = 1; k < FB_SCALE; k++)
				memcpy(framebuffer + (FB_MAJSTRIDE * i + FB_SCALE * x + k * FB_WIDTH), framebuffer +
					FB_MAJSTRIDE * i + FB_SCALE * x, FB_SCALE * w * sizeof(uint32_t));
		}
	}

	void render_framebuffer_vline(uint16_t x, uint16_t y1, uint16_t y2, uint32_t color)
	{
		for(unsigned i = y1; i <= y2 && i < 200; i++) {
			for(unsigned k = 0; k < FB_SCALE; k++)
				framebuffer[FB_MAJSTRIDE * i + x + k * FB_WIDTH] = 0xFF000000U | color;
		}
	}

	char decode(const char* ch)
	{
		char c = *ch;
		if(c >= '\\')
			c--;
		return c - 35;
	}

	void draw_block2(const char* pointer, uint16_t position, uint32_t c1, uint32_t c2, bool zero)
	{
		static uint8_t tbl[3][6] = {{27, 9, 3, 1}, {32, 16, 8, 4, 2, 1}, {32, 16, 8, 4, 2, 1}};
		static uint8_t tbl2[3][3] = {{4, 6, 6}, {3, 2, 2}, {0, 0, 1}};
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
					origbuffer[position] = (p > 1) ? c2 : c1;
				else if(zero)
					origbuffer[position] = 0;
				position++;
			}
			position += (320 - width);
		}
		render_framebuffer_update(origptr % 320, origptr / 320, width, height);
	}

	void draw_block(const uint8_t* pointer, uint16_t position, uint32_t c1, uint32_t c2, bool zero)
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
					origbuffer[position] = (p > 1) ? c2 : c1;
				else if(zero)
					origbuffer[position] = 0;
				position++;
			}
			position += (320 - width);
		}
		render_framebuffer_update(origptr % 320, origptr / 320, width, height);
	}

	void draw_message(const char* pointer, uint32_t c1, uint32_t c2)
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
		draw_block2(op, p, c1, c2);
	}
	
	void draw_distance_column(uint16_t col, uint32_t c)
	{
		uint16_t minline = 0x8f;
		uint16_t maxline = 0x8f;
		uint16_t ptr = 320 * 0x8f + 0x2a + (col / FB_SCALE);
		uint32_t px = origbuffer[ptr] ;
		while(origbuffer[ptr] == px)
			ptr -= 320;
		ptr += 320;
		minline = ptr / 320;
		while(origbuffer[ptr] == px)
			ptr += 320;
		maxline = ptr / 320 - 1;
		render_framebuffer_vline(col + FB_SCALE * 0x2a, minline, maxline, c | 0xFF000000U);
	}

	void blink_between(unsigned x, unsigned y, unsigned w, unsigned h, uint32_t c1, uint32_t c2)
	{
		c1 &= 0xFFFFFF;
		c2 &= 0xFFFFFF;
		uint32_t c1x = c1 | 0xFF000000U;
		uint32_t c2x = c2 | 0xFF000000U;
		for(unsigned j = y; j < y + h; j++)
			for(unsigned i = x; i < x + w; i++) {
				if((origbuffer[320 * j + i] & 0xFFFFFF) == c1)
					origbuffer[320 * j + i] = c2x;
				else if((origbuffer[320 * j + i] & 0xFFFFFF) == c2)
					origbuffer[320 * j + i] = c1x;
			}
		render_framebuffer_update(x, y, w, h);
	}

	void draw_bitmap(const uint32_t* bitmap, unsigned x, unsigned y)
	{
		for(unsigned j = 0; j < bitmap[1]; j++)
			for(unsigned i = 0; i < bitmap[0]; i++)
				framebuffer[(y + j) * FB_WIDTH + (x + i)] = bitmap[j * bitmap[0] + i + 2];
	}
}
