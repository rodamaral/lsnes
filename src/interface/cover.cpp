#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/memorymanip.hpp"
#include "core/rom.hpp"
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
		const framebuffer::font::glyph& g = main_font.get_glyph(ch);
		unsigned maxw = min((size_t)g.get_width(), (size_t)(w - x));
		unsigned maxh = min((size_t)g.get_height(),  (size_t)(h - y));
		unsigned char* cellbase = reinterpret_cast<unsigned char*>(fb) + y * istride + pstride * x;
		for(size_t y2 = 0; y2 < maxh; y2++) {
			for(size_t j = 0; j < maxw; j++) {
				if(g.read_pixel(j, y2)) {
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
	utf8::to32i(str.begin(), str.end(), lambda_output_iterator<int32_t>([fb, &x, &y, fg, bg, w, h, istride,
		pstride](int32_t u) {
		if(u != 9 && u != 10)
			cover_render_character(fb, x, y, u, fg, bg, w, h, istride, pstride);
		cover_next_position(u, x, y);
	}));
}

void cover_next_position(uint32_t ch, unsigned& x, unsigned& y)
{
	if(ch == 9)
		x = (x + 63) >> 6 << 6;
	else if(ch == 10) {
		x = 0;
		y = y + 16;
	} else {
		const framebuffer::font::glyph& g = main_font.get_glyph(ch);
		x = x + g.get_width();
	}
}

void cover_next_position(const std::string& str, unsigned& x, unsigned& y)
{
	utf8::to32i(str.begin(), str.end(), lambda_output_iterator<int32_t>([&x, &y](int32_t u) {
		cover_next_position(u, x, y);
	}));
}

std::vector<std::string> cover_information()
{
	auto& core = CORE();
	std::vector<std::string> ret;
	ret.push_back("System: " + core.rom->get_hname() + " (" + core.rom->region_get_hname() + ")");
	return ret;
}
