#include "core/moviedata.hpp"
#include "interface/cover.hpp"
#include "library/minmax.hpp"
#include "library/utf8.hpp"
#include "fonts/wrapper.hpp"
#include <iostream>

namespace
{
	template<size_t pstride>
	void cover_render_character(void* fb, unsigned x, unsigned y, uint32_t ch, uint32_t fg, uint32_t bg,
		size_t w, size_t h, size_t istride)
	{
		unsigned char* _fg = reinterpret_cast<unsigned char*>(&fg);
		unsigned char* _bg = reinterpret_cast<unsigned char*>(&bg);
		if(x >= w || y >= h)
			return;
		const bitmap_font::glyph& g = main_font.get_glyph(ch);
		unsigned maxw = min((size_t)(g.wide ? 16 : 8), (size_t)(w - x));
		unsigned maxh = min((size_t)16,  (size_t)(h - y));
		unsigned char* cellbase = reinterpret_cast<unsigned char*>(fb) + y * istride + pstride * x;
		if(g.wide) {
			//Wide character.
			for(size_t y2 = 0; y2 < maxh; y2++) {
				uint32_t d = g.data[y2 >> 1];
				d >>= 16 - ((y2 & 1) << 4);
				for(size_t j = 0; j < maxw; j++) {
					uint32_t b = 15 - j;
					if(((d >> b) & 1) != 0) {
						for(unsigned k = 0; k < pstride; k++)
							cellbase[pstride * j + k] = _fg[k];
					} else {
						for(unsigned k = 0; k < pstride; k++)
							cellbase[pstride * j + k] = _bg[k];
					}
				}
				cellbase += istride;
			}
		} else {
			//Narrow character.
			for(size_t y2 = 0; y2 < maxh; y2++) {
				uint32_t d = g.data[y2 >> 2];
				d >>= 24 - ((y2 & 3) << 3);
				for(size_t j = 0; j < maxw; j++) {
					uint32_t b = 7 - j;
					if(((d >> b) & 1) != 0) {
						for(unsigned k = 0; k < pstride; k++)
							cellbase[pstride * j + k] = _fg[k];
					} else {
						for(unsigned k = 0; k < pstride; k++)
							cellbase[pstride * j + k] = _bg[k];
					}
				}
				cellbase += istride;
			}
		}
	}
}

void cover_render_character(void* fb, unsigned x, unsigned y, uint32_t ch, uint32_t fg, uint32_t bg, size_t w,
	size_t h, size_t istride, size_t pstride)
{
	if(pstride == 2)
		cover_render_character<2>(fb, x, y, ch, fg, bg, w, h, istride); 
	if(pstride == 3)
		cover_render_character<3>(fb, x, y, ch, fg, bg, w, h, istride); 
	if(pstride == 4)
		cover_render_character<4>(fb, x, y, ch, fg, bg, w, h, istride); 
}

void cover_render_string(void* fb, unsigned x, unsigned y, const std::string& str, uint32_t fg, uint32_t bg,
	size_t w, size_t h, size_t istride, size_t pstride)
{
	size_t spos = 0;
	size_t slen = str.length();
	uint16_t state = utf8_initial_state;
	while(true) {
		int ch = (spos < slen) ? (unsigned char)str[spos] : - 1;
		int32_t u = utf8_parse_byte(ch, state);
		if(u < 0) {
			if(ch < 0)
				break;
			spos++;
			continue;
		}
		cover_render_character(fb, x, y, u, fg, bg, w, h, istride, pstride);
		cover_next_position(u, x, y);
		spos++;
	}
}

void cover_next_position(uint32_t ch, unsigned& x, unsigned& y)
{
	unsigned ox = x;
	unsigned oy = y;
	if(ch == 9)
		x = (x + 63) >> 6 << 6;
	else if(ch == 10) {
		x = 0;
		y = y + 16;
	} else {
		const bitmap_font::glyph& g = main_font.get_glyph(ch);
		x = x + (g.wide ? 16 : 8);
	}
}

void cover_next_position(const std::string& str, unsigned& x, unsigned& y)
{
	size_t spos = 0;
	size_t slen = str.length();
	uint16_t state = utf8_initial_state;
	while(true) {
		int ch = (spos < slen) ? (unsigned char)str[spos] : - 1;
		int32_t u = utf8_parse_byte(ch, state);
		if(u < 0) {
			if(ch < 0)
				break;
			spos++;
			continue;
		}
		cover_next_position(u, x, y);
		spos++;
	}
}

std::vector<std::string> cover_information()
{
	std::vector<std::string> ret;
	ret.push_back("Region: " + our_rom->region->get_hname());
	return ret;
}