#include "lsnes.hpp"
#include <snes/snes.hpp>

#include "render.hpp"
#include "png.hpp"
#include <sstream>
#include <list>
#include <iomanip>
#include <cstdint>
#include <string>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
#include <nall/platform.hpp>
#include <nall/endian.hpp>
#include <nall/varint.hpp>
#include <nall/bit.hpp>
#include <nall/serializer.hpp>
#include <nall/property.hpp>
using namespace nall;
#include <ui-libsnes/libsnes.hpp>
using namespace SNES;


extern uint32_t fontdata[];

namespace
{
	inline uint32_t blend(uint32_t orig, uint16_t ialpha, uint32_t pl, uint32_t ph) throw()
	{
		const uint32_t X = 0xFF00FFU;
		const uint32_t Y = 0xFF00FF00U;
		return ((ialpha * ((orig >> 8) & X) + ph) & Y) | (((ialpha * (orig & X) + pl)
			>> 8) & X);
	}

	//This is Jenkin's MIX function.
	uint32_t keyhash(uint32_t key, uint32_t item, uint32_t mod) throw()
	{
		uint32_t a = key;
		uint32_t b = 0;
		uint32_t c = item;
		a=a-b;	a=a-c;	a=a^(c >> 13);
		b=b-c;	b=b-a;	b=b^(a << 8);
		c=c-a;	c=c-b;	c=c^(b >> 13);
		a=a-b;	a=a-c;	a=a^(c >> 12);
		b=b-c;	b=b-a;	b=b^(a << 16);
		c=c-a;	c=c-b;	c=c^(b >> 5);
		a=a-b;	a=a-c;	a=a^(c >> 3);
		b=b-c;	b=b-a;	b=b^(a << 10);
		c=c-a;	c=c-b;	c=c^(b >> 15);
		return c % mod;
	}
}

//Locate glyph in font. Returns <width, offset> pair. Zero offset should be interpretted as an empty
//glyph.
std::pair<uint32_t, size_t> find_glyph(uint32_t codepoint, int32_t x, int32_t y, int32_t orig_x,
	int32_t& next_x, int32_t& next_y) throw()
{
	uint32_t cwidth = 0;
	if(codepoint == 9) {
		cwidth = 64 - (x - orig_x) % 64;
		next_x = x + cwidth;
		next_y = y;
		return std::make_pair(cwidth, 0);
	} else if(codepoint == 10) {
		next_x = orig_x;
		next_y = y + 16;
		return std::make_pair(0, 0);
	} else if(codepoint == 32) {
		next_x = x + 8;
		next_y = y;
		return std::make_pair(8, 0);
	} else {
		uint32_t mdir = fontdata[0];
		uint32_t mseed = fontdata[mdir];
		uint32_t msize = fontdata[mdir + 1];
		uint32_t midx = keyhash(mseed, codepoint, msize);
		uint32_t sdir = fontdata[mdir + 2 + midx];
		if(!fontdata[sdir + 1]) {
			//Character not found.
			next_x = x + 8;
			next_y = y;
			return std::make_pair(8, 0);
		}
		uint32_t sseed = fontdata[sdir];
		uint32_t ssize = fontdata[sdir + 1];
		uint32_t sidx = keyhash(sseed, codepoint, ssize);
		if(fontdata[sdir + 2 + 2 * sidx] != codepoint) {
			//Character not found.
			next_x = x + 8;
			next_y = y;
			return std::make_pair(8, 0);
		}
		bool wide = (fontdata[fontdata[sdir + 2 + 2 * sidx + 1]] != 0);
		next_x = x + (wide ? 16 : 8);
		next_y = y;
		return std::make_pair(wide ? 16 : 8, fontdata[sdir + 2 + 2 * sidx + 1] + 1);
	}
}

render_object::~render_object() throw()
{
}

void render_text(struct screen& scr, int32_t x, int32_t y, const std::string& text, uint32_t fg,
	uint16_t fgalpha, uint32_t bg, uint16_t bgalpha) throw(std::bad_alloc)
{
	uint32_t pfgl = (fg & 0xFF00FF) * fgalpha;
	uint32_t pfgh = ((fg >> 8) & 0xFF00FF) * fgalpha;
	uint32_t pbgl = (bg & 0xFF00FF) * bgalpha;
	uint32_t pbgh = ((bg >> 8) & 0xFF00FF) * bgalpha;
	uint16_t ifga = 256 - fgalpha;
	uint16_t ibga = 256 - bgalpha;
	int32_t orig_x = x;
	uint32_t unicode_code = 0;
	uint8_t unicode_left = 0;
	for(size_t i = 0; i < text.length(); i++) {
		uint8_t ch = text[i];
		if(ch < 128)
			unicode_code = text[i];
		else if(ch < 192) {
				if(!unicode_left)
					continue;
				unicode_code = 64 * unicode_code + ch - 128;
				if(--unicode_left)
					continue;
		} else if(ch < 224) {
			unicode_code = ch - 192;
			unicode_left = 1;
			continue;
		} else if(ch < 240) {
			unicode_code = ch - 224;
			unicode_left = 2;
			continue;
		} else if(ch < 248) {
			unicode_code = ch - 240;
			unicode_left = 3;
			continue;
		} else
			continue;
		int32_t next_x, next_y;
		auto p = find_glyph(unicode_code, x, y, orig_x, next_x, next_y);
		uint32_t dx = 0;
		uint32_t dw = p.first;
		uint32_t dy = 0;
		uint32_t dh = 16;
		uint32_t cx = static_cast<uint32_t>(static_cast<int32_t>(scr.originx) + x);
		uint32_t cy = static_cast<uint32_t>(static_cast<int32_t>(scr.originy) + y);
		while(cx > scr.width && dw > 0) {
			dx++;
			dw--;
			cx++;
		}
		while(cy > scr.height && dh > 0) {
			dy++;
			dh--;
			cy++;
		}
		while(cx + dw > scr.width && dw > 0)
			dw--;
		while(cy + dh > scr.height && dh > 0)
			dh--;
		if(!dw || !dh)
			continue;	//Outside screen.

		if(p.second == 0) {
			//Blank glyph.
			for(uint32_t j = 0; j < dh; j++) {
				uint32_t* base = scr.rowptr(cy + j) + cx;
				for(uint32_t i = 0; i < dw; i++)
					base[i] = blend(base[i], ibga, pbgl, pbgh);
			}
		} else {
			//narrow/wide glyph.
			for(uint32_t j = 0; j < dh; j++) {
				uint32_t dataword = fontdata[p.second + (dy + j) / (32 / p.first)];
				uint32_t* base = scr.rowptr(cy + j) + cx;
				for(uint32_t i = 0; i < dw; i++)
					if(((dataword >> (31 - ((dy + j) % (32 / p.first)) * p.first - (dx + i))) & 1))
						base[i] = blend(base[i], ifga, pfgl, pfgh);
					else
						base[i] = blend(base[i], ibga, pbgl, pbgh);
			}
		}
		x = next_x;
		y = next_y;
	}
}

void render_queue::add(struct render_object& obj) throw(std::bad_alloc)
{
	q.push_back(&obj);
}

void render_queue::run(struct screen& scr) throw()
{
	for(auto i = q.begin(); i != q.end(); i++) {
		try {
			(**i)(scr);
		} catch(...) {
		}
		delete *i;
	}
	q.clear();
}

void render_queue::clear() throw()
{
	for(auto i = q.begin(); i != q.end(); i++)
		delete *i;
	q.clear();
}

render_queue::~render_queue() throw()
{
	clear();
}

uint32_t screen::make_color(uint8_t r, uint8_t g, uint8_t b) throw()
{
	return (static_cast<uint32_t>(r) << active_rshift) |
		(static_cast<uint32_t>(g) << active_gshift) |
		(static_cast<uint32_t>(b) << active_bshift);
}

lcscreen::lcscreen(const uint16_t* mem, bool hires, bool interlace, bool overscan, bool region) throw()
{
	uint32_t dataoffset = 0;
	width = hires ? 512 : 256;
	height = 0;
	if(region) {
		//PAL.
		height = 239;
		dataoffset = overscan ? 9 : 1;
	} else {
		//presumably NTSC.
		height = 224;
		dataoffset = overscan ? 16 : 9;
	}
	if(interlace)
		height <<= 1;
	memory = mem + dataoffset * 1024;
	pitch = interlace ? 512 : 1024;
	user_memory = false;
}

lcscreen::lcscreen(const uint16_t* mem, uint32_t _width, uint32_t _height) throw()
{
	width = _width;
	height = _height;
	memory = mem;
	pitch = width;
	user_memory = false;
}

lcscreen::lcscreen() throw()
{
	width = 0;
	height = 0;
	memory = NULL;
	user_memory = true;
	pitch = 0;
	allocated = 0;
}

lcscreen::lcscreen(const lcscreen& ls) throw(std::bad_alloc)
{
	width = ls.width;
	height = ls.height;
	pitch = width;
	user_memory = true;
	allocated = static_cast<size_t>(width) * height;
	memory = new uint16_t[allocated];
	for(size_t l = 0; l < height; l++)
		memcpy(const_cast<uint16_t*>(memory + l * width), ls.memory + l * ls.pitch, 2 * width);
}

lcscreen& lcscreen::operator=(const lcscreen& ls) throw(std::bad_alloc, std::runtime_error)
{
	if(!user_memory)
		throw std::runtime_error("Can't copy to non-user memory");
	if(this == &ls)
		return *this;
	if(allocated < static_cast<size_t>(ls.width) * ls.height) {
		size_t p_allocated = static_cast<size_t>(ls.width) * ls.height;
		memory = new uint16_t[p_allocated];
		allocated = p_allocated;
	}
	width = ls.width;
	height = ls.height;
	pitch = width;
	for(size_t l = 0; l < height; l++)
		memcpy(const_cast<uint16_t*>(memory + l * width), ls.memory + l * ls.pitch, 2 * width);
	return *this;
}

lcscreen::~lcscreen()
{
	if(user_memory)
		delete[] const_cast<uint16_t*>(memory);
}

void lcscreen::load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error)
{
	if(!user_memory)
		throw std::runtime_error("Can't load to non-user memory");
	const uint8_t* data2 = reinterpret_cast<const uint8_t*>(&data[0]);
	if(data.size() < 2) 
		throw std::runtime_error("Corrupt saved screenshot data");
	uint32_t _width = static_cast<uint32_t>(data2[0]) * 256 + static_cast<uint32_t>(data2[1]);
	if(_width > 1 && data.size() % (2 * _width) != 2)
		throw std::runtime_error("Corrupt saved screenshot data");
	uint32_t _height = (data.size() - 2) / (2 * _width);
	if(allocated < static_cast<size_t>(_width) * _height) {
		size_t p_allocated = static_cast<size_t>(_width) * _height;
		memory = new uint16_t[p_allocated];
		allocated = p_allocated;
	}
	uint16_t* mem = const_cast<uint16_t*>(memory);
	width = _width;
	height = _height;
	pitch = width;
	for(size_t i = 0; i < (data.size() - 2) / 2; i++)
		mem[i] = static_cast<uint16_t>(data2[2 + 2 * i]) * 256 +
			static_cast<uint16_t>(data2[2 + 2 * i + 1]);
}

void lcscreen::save(std::vector<char>& data) throw(std::bad_alloc)
{
	data.resize(2 + 2 * static_cast<size_t>(width) * height);
	uint8_t* data2 = reinterpret_cast<uint8_t*>(&data[0]);
	data2[0] = (width >> 8);
	data2[1] = width;
	for(size_t i = 0; i < (data.size() - 2) / 2; i++) {
		data[2 + 2 * i] = memory[(i / width) * pitch + (i % width)] >> 8;
		data[2 + 2 * i + 1] = memory[(i / width) * pitch + (i % width)];
	}
}

void lcscreen::save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	unsigned char clevels[32];
	for(unsigned i = 0; i < 32; i++)
		clevels[i] = 255 * i / 31;
	uint8_t* buffer = new uint8_t[3 * static_cast<size_t>(width) * height];
	for(uint32_t j = 0; j < height; j++)
		for(uint32_t i = 0; i < width; i++) {
			uint16_t word = memory[pitch * j + i];
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 0] = clevels[(word >> 10) & 0x1F];
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 1] = clevels[(word >> 5) & 0x1F];
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 2] = clevels[(word) & 0x1F];
		}
	try {
		save_png_data(file, buffer, width, height);
		delete[] buffer;
	} catch(...) {
		delete[] buffer;
		throw;
	}
}

void screen::copy_from(lcscreen& scr, uint32_t hscale, uint32_t vscale) throw()
{
	uint32_t copyable_width = (width - originx) / hscale;
	uint32_t copyable_height = (height - originy) / vscale;
	copyable_width = (copyable_width > scr.width) ? scr.width : copyable_width;
	copyable_height = (copyable_height > scr.height) ? scr.height : copyable_height;
	for(uint32_t y = 0; y < height; y++) {
		memset(rowptr(y), 0, 4 * width);
	}
	for(uint32_t y = 0; y < copyable_height; y++) {
		uint32_t line = y * vscale + originy;
		uint32_t* ptr = rowptr(line) + originx;
		const uint16_t* sbase = scr.memory + y * scr.pitch;
		for(uint32_t x = 0; x < copyable_width; x++) {
			uint32_t c = palette[sbase[x] % 32768];
			for(uint32_t i = 0; i < hscale; i++)
				*(ptr++) = c;
		}
		for(uint32_t j = 1; j < vscale; j++)
			memcpy(rowptr(line + j) + originx, rowptr(line) + originx, 4 * hscale * copyable_width);
	}
}

void screen::reallocate(uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy, bool upside_down)
	throw(std::bad_alloc)
{
	if(_width == width && _height == height) {
		originx = _originx;
		originy = _originy;
		return;
	}
	if(!_width || !_height) {
		width = height = originx = originy = pitch = 0;
		if(memory && !user_memory)
			delete[] memory;
		memory = NULL;
		user_memory = false;
		flipped = upside_down;
		return;
	}
	uint32_t* newmem = new uint32_t[_width * _height];
	width = _width;
	height = _height;
	originx = _originx;
	originy = _originy;
	pitch = 4 * _width;
	if(memory && !user_memory)
		delete[] memory;
	memory = newmem;
	user_memory = false;
	flipped = upside_down;
}

void screen::set(uint32_t* _memory, uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy,
	uint32_t _pitch) throw()
{
	if(memory && !user_memory)
		delete[] memory;
	width = _width;
	height = _height;
	originx = _originx;
	originy = _originy;
	pitch = _pitch;
	user_memory = true;
	memory = _memory;
	flipped = false;
}

uint32_t* screen::rowptr(uint32_t row) throw()
{
	if(flipped)
		row = height - row - 1;
	return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(memory) + row * pitch);
}

screen::screen() throw()
{
	uint32_t _magic = 403703808;
	uint8_t* magic = reinterpret_cast<uint8_t*>(&_magic);
	memory = NULL;
	width = height = originx = originy = pitch = 0;
	user_memory = false;
	flipped = false;
	active_rshift = active_gshift = active_bshift = 255;
	set_palette(magic[0], magic[1], magic[2]);
}

screen::~screen() throw()
{
	if(memory && !user_memory)
		delete[] memory;
}

void screen::set_palette(uint32_t rshift, uint32_t gshift, uint32_t bshift) throw()
{
	if(rshift == active_rshift && gshift == active_gshift && bshift == active_bshift)
		return;
	uint32_t old_rshift = active_rshift;
	uint32_t old_gshift = active_gshift;
	uint32_t old_bshift = active_bshift;
	uint32_t xpalette[32];
	for(unsigned i = 0; i < 32; i++)
		xpalette[i] = (i * 255 / 31);
	for(unsigned i = 0; i < 32768; i++) {
		palette[i] = (xpalette[(i >> 10) & 31] << rshift) +
			(xpalette[(i >> 5) & 31] << gshift) +
			(xpalette[i & 31] << bshift);
	}
	active_rshift = rshift;
	active_gshift = gshift;
	active_bshift = bshift;
	//Convert the data.
	for(uint32_t j = 0; j < height; j++) {
		uint32_t* rp = rowptr(j);
		for(uint32_t i = 0; i < width; i++) {
			uint32_t x = rp[i];
			uint32_t r = (x >> old_rshift) & 0xFF;
			uint32_t g = (x >> old_gshift) & 0xFF;
			uint32_t b = (x >> old_bshift) & 0xFF;
			x = (r << active_rshift) | (g << active_gshift) | (b << active_bshift);
			rp[i] = x;
		}
	}
}
