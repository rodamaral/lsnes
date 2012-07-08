#include "core/misc.hpp"
#include "fonts/wrapper.hpp"
#include "platform/sdl/platform.hpp"

#include <cstdint>
#include <vector>
#include <string>

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define WRITE3(ptr, idx, c) do {\
	(ptr)[(idx) + 0] = (c) >> 16; \
	(ptr)[(idx) + 1] = (c) >> 8; \
	(ptr)[(idx) + 2] = (c); \
	} while(0)
#else
#define WRITE3(ptr, idx, c) do {\
	(ptr)[(idx) + 0] = (c); \
	(ptr)[(idx) + 1] = (c) >> 8; \
	(ptr)[(idx) + 2] = (c) >> 16; \
	} while(0)
#endif

std::vector<uint32_t> decode_utf8(std::string s)
{
	std::vector<uint32_t> ret;
	for(auto i = s.begin(); i != s.end(); i++) {
		uint32_t j = static_cast<uint8_t>(*i);
		if(j < 128)
			ret.push_back(j);
		else if(j < 192)
			continue;
		else if(j < 224) {
			uint32_t j2 = static_cast<uint8_t>(*(++i));
			ret.push_back((j - 192) * 64 + (j2 - 128));
		} else if(j < 240) {
			uint32_t j2 = static_cast<uint8_t>(*(++i));
			uint32_t j3 = static_cast<uint8_t>(*(++i));
			ret.push_back((j - 224) * 4096 + (j2 - 128) * 64 + (j3 - 128));
		} else {
			uint32_t j2 = static_cast<uint8_t>(*(++i));
			uint32_t j3 = static_cast<uint8_t>(*(++i));
			uint32_t j4 = static_cast<uint8_t>(*(++i));
			ret.push_back((j - 240) * 262144 + (j2 - 128) * 4096 + (j3 - 128) * 64 + (j4 - 128));
		}
	}
	return ret;
}

namespace
{

	inline void draw_blank_glyph_3(uint8_t* base, uint32_t pitch, uint32_t w, uint32_t color, uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			uint8_t* ptr = base + j * pitch;
			uint32_t c = (j >= curstart) ? color : 0;
			for(uint32_t i = 0; i < w; i++)
				WRITE3(ptr, 3 * i, c);
		}
	}

	template<typename T>
	inline void draw_blank_glyph_T(uint8_t* base, uint32_t pitch, uint32_t w, uint32_t color, uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			T* ptr = reinterpret_cast<T*>(base + j * pitch);
			T c = (j >= curstart) ? color : 0;
			for(uint32_t i = 0; i < w; i++)
				ptr[i] = c;
		}
	}

	template<bool wide>
	inline void draw_glyph_3(uint8_t* base, uint32_t pitch, uint32_t w, const uint32_t* gdata, uint32_t color,
		uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			uint8_t* ptr = base + j * pitch;
			uint32_t bgc = (j >= curstart) ? color : 0;
			uint32_t fgc = (j >= curstart) ? 0 : color;
			uint32_t dataword = gdata[j >> (wide ? 1 : 2)];
			unsigned rbit = (~(j << (wide ? 4 : 3))) & 0x1F;
			for(uint32_t i = 0; i < w; i++) {
				bool b = (((dataword >> (rbit - i)) & 1));
				WRITE3(ptr, 3 * i, b ? fgc : bgc);
			}
		}
	}

	template<typename T, bool wide>
	inline void draw_glyph_T(uint8_t* base, uint32_t pitch, uint32_t w, const uint32_t* gdata, uint32_t color,
		uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			T* ptr = reinterpret_cast<T*>(base + j * pitch);
			T bgc = (j >= curstart) ? color : 0;
			T fgc = (j >= curstart) ? 0 : color;
			uint32_t dataword = gdata[j >> (wide ? 1 : 2)];
			unsigned rbit = (~(j << (wide ? 4 : 3))) & 0x1F;
			for(uint32_t i = 0; i < w; i++) {
				bool b = (((dataword >> (rbit - i)) & 1));
				ptr[i] = b ? fgc : bgc;
			}
		}
	}

	void draw_blank_glyph(uint8_t* base, uint32_t pitch, uint32_t pbytes, uint32_t w, uint32_t wleft,
		uint32_t color, uint32_t hilite_mode)
	{
		if(w > wleft)
			w = wleft;
		if(!w)
			return;
		uint32_t curstart = 16;
		if(hilite_mode == 1)
			curstart = 14;
		if(hilite_mode == 2)
			curstart = 0;
		switch(pbytes) {
		case 1:
			draw_blank_glyph_T<uint8_t>(base, pitch, w, color, curstart);
			break;
		case 2:
			draw_blank_glyph_T<uint16_t>(base, pitch, w, color, curstart);
			break;
		case 3:
			draw_blank_glyph_3(base, pitch, w, color, curstart);
			break;
		case 4:
			draw_blank_glyph_T<uint32_t>(base, pitch, w, color, curstart);
			break;
		}
	}

	void draw_glyph(uint8_t* base, uint32_t pitch, uint32_t pbytes, const uint32_t* gdata, uint32_t w,
		uint32_t wleft, uint32_t color, uint32_t hilite_mode)
	{
		bool wide = (w > 8);
		if(w > wleft)
			w = wleft;
		if(!w)
			return;
		uint32_t curstart = 16;
		if(hilite_mode == 1)
			curstart = 14;
		if(hilite_mode == 2)
			curstart = 0;
		switch(pbytes) {
		case 1:
			if(wide)
				draw_glyph_T<uint8_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint8_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		case 2:
			if(wide)
				draw_glyph_T<uint16_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint16_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		case 3:
			if(wide)
				draw_glyph_3<true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_3<false>(base, pitch, w, gdata, color, curstart);
			break;
		case 4:
			if(wide)
				draw_glyph_T<uint32_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint32_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		}
	}

	void draw_string(uint8_t* base, uint32_t pitch, uint32_t pbytes, std::vector<uint32_t> s, uint32_t x,
		uint32_t y, uint32_t maxwidth, uint32_t color, uint32_t hilite_mode = 0, uint32_t hilite_pos = 0)
	{
		int32_t pos_x = 0;
		int32_t pos_y = 0;
		size_t xo = pbytes;
		size_t yo = pitch;
		unsigned c = 0;
		for(auto si : s) {
			uint32_t old_x = pos_x;
			uint32_t old_y = pos_y;
			auto g = main_font.get_glyph(si);
			if(si == 9)
				pos_x = (pos_x + 63) / 64 * 64;
			else if(si == 10) {
				pos_x = 0;
				pos_y += 16;
			} else
				pos_x += (g.wide ? 16 : 8);
			uint32_t mw = maxwidth - old_x;
			if(maxwidth < old_x)
				mw = 0;
			if(mw > (g.wide ? 16 : 8))
				mw = (g.wide ? 16 : 8);
			uint8_t* cbase = base + (y + old_y) * yo + (x + old_x) * xo;
			if(g.data == NULL)
				draw_blank_glyph(cbase, pitch, pbytes, (g.wide ? 16 : 8), mw, color,
					(c == hilite_pos) ? hilite_mode : 0);
			else
				draw_glyph(cbase, pitch, pbytes, g.data, (g.wide ? 16 : 8), mw, color,
					(c == hilite_pos) ? hilite_mode : 0);
			c++;
		}
		if(c == hilite_pos) {
			uint32_t old_x = pos_x;
			uint32_t mw = maxwidth - old_x;
			if(maxwidth < old_x)
				mw = 0;
			draw_blank_glyph(base + y * yo + (x + old_x) * xo, pitch, pbytes, 8, mw, 0xFFFFFFFFU,
				hilite_mode);
			pos_x += 8;
		}
		if(pos_x < maxwidth)
			draw_blank_glyph(base + y * yo + (x + pos_x) * xo, pitch, pbytes, maxwidth - pos_x,
				maxwidth - pos_x, 0, 0);
	}

	void paint_line(uint8_t* ptr, uint32_t length, uint32_t step, uint32_t pbytes, uint32_t color)
	{
		switch(pbytes) {
		case 1:
			for(uint32_t i = 0; i < length; i++) {
				*ptr = color;
				ptr += step;
			}
			break;
		case 2:
			for(uint32_t i = 0; i < length; i++) {
				*reinterpret_cast<uint16_t*>(ptr) = color;
				ptr += step;
			}
			break;
		case 3:
			for(uint32_t i = 0; i < length; i++) {
				WRITE3(ptr, 0, color);
				ptr += step;
			}
			break;
		case 4:
			for(uint32_t i = 0; i < length; i++) {
				*reinterpret_cast<uint32_t*>(ptr) = color;
				ptr += step;
			}
			break;
		}
	}
}

void draw_string(uint8_t* base, uint32_t pitch, uint32_t pbytes, std::string s, uint32_t x, uint32_t y,
	uint32_t maxwidth, uint32_t color, uint32_t hilite_mode, uint32_t hilite_pos) throw()
{
	try {
		draw_string(base, pitch, pbytes, decode_utf8(s), x, y, maxwidth, color, hilite_mode, hilite_pos);
	} catch(...) {
		OOM_panic();
	}
}

void draw_string(SDL_Surface* surf, std::string s, uint32_t x, uint32_t y, uint32_t maxwidth, uint32_t color,
	uint32_t hilite_mode, uint32_t hilite_pos) throw()
{
	try {
		draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, surf->format->BytesPerPixel,
			decode_utf8(s), x, y, maxwidth, color, hilite_mode, hilite_pos);
	} catch(...) {
		OOM_panic();
	}
}

void draw_box(uint8_t* base, uint32_t pitch, uint32_t pbytes, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, uint32_t color) throw()
{
	if(!w || !h)
		return;
	uint32_t sx, sy, ex, ey;
	uint32_t bsx, bsy, bex, bey;
	uint32_t lstride = pitch;
	uint8_t* p = base;
	size_t xo = pbytes;
	size_t yo = pitch;
	sx = (x < 6) ? 0 : (x - 6);
	sy = (y < 6) ? 0 : (y - 6);
	ex = (x + w + 6 > width) ? width : (x + w + 6);
	ey = (y + h + 6 > height) ? height : (y + h + 6);
	bsx = (x < 4) ? 0 : (x - 4);
	bsy = (y < 4) ? 0 : (y - 4);
	bex = (x + w + 4 > width) ? width : (x + w + 4);
	bey = (y + h + 4 > height) ? height : (y + h + 4);
	//First, blank the area.
	for(uint32_t j = sy; j < ey; j++)
		memset(p + j * yo + sx * xo, 0, (ex - sx) * xo);
	//Paint the borders.
	if(x >= 4)
		paint_line(p + bsy * yo + (x - 4) * xo, bey - bsy, yo, xo, color);
	if(x >= 3)
		paint_line(p + bsy * yo + (x - 3) * xo, bey - bsy, yo, xo, color);
	if(y >= 4)
		paint_line(p + (y - 4) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(y >= 3)
		paint_line(p + (y - 3) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(x + w + 3 < width)
		paint_line(p + bsy * yo + (x + w + 2) * xo, bey - bsy, yo, xo, color);
	if(x + w + 4 < width)
		paint_line(p + bsy * yo + (x + w + 3) * xo, bey - bsy, yo, xo, color);
	if(y + h + 3 < height)
		paint_line(p + (y + h + 2) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(y + h + 4 < height)
		paint_line(p + (y + h + 3) * yo + bsx * xo, bex - bsx, xo, xo, color);
}

void draw_box(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) throw()
{
	draw_box(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, surf->format->BytesPerPixel, surf->w, surf->h,
		x, y, w, h, color);
}
