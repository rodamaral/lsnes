#include "lsnes.hpp"
#include <snes/snes.hpp>

#include "core/png.hpp"
#include "core/render.hpp"

#include <sstream>
#include <list>
#include <iomanip>
#include <cstdint>
#include <string>

extern uint32_t fontdata[];

namespace
{
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

void render_text(struct screen& scr, int32_t x, int32_t y, const std::string& text, premultiplied_color fg,
	premultiplied_color bg) throw(std::bad_alloc)
{
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
					bg.apply(base[i]);
			}
		} else {
			//narrow/wide glyph.
			for(uint32_t j = 0; j < dh; j++) {
				uint32_t dataword = fontdata[p.second + (dy + j) / (32 / p.first)];
				uint32_t* base = scr.rowptr(cy + j) + cx;
				for(uint32_t i = 0; i < dw; i++)
					if(((dataword >> (31 - ((dy + j) % (32 / p.first)) * p.first - (dx + i))) & 1))
						fg.apply(base[i]);
					else
						bg.apply(base[i]);
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
	for(auto i : q) {
		try {
			(*i)(scr);
		} catch(...) {
		}
		delete i;
	}
	q.clear();
}

void render_queue::clear() throw()
{
	for(auto i : q)
		delete i;
	q.clear();
}

render_queue::~render_queue() throw()
{
	clear();
}

uint32_t screen::make_color(uint8_t r, uint8_t g, uint8_t b) throw()
{
	uint32_t _r = r;
	uint32_t _g = g;
	uint32_t _b = b;
	return (_r << 16) + (_g << 8) + _b;
}

lcscreen::lcscreen(const uint32_t* mem, bool hires, bool interlace, bool overscan, bool region) throw()
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

lcscreen::lcscreen(const uint32_t* mem, uint32_t _width, uint32_t _height) throw()
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
	memory = new uint32_t[allocated];
	for(size_t l = 0; l < height; l++)
		memcpy(const_cast<uint32_t*>(memory + l * width), ls.memory + l * ls.pitch, 4 * width);
}

lcscreen& lcscreen::operator=(const lcscreen& ls) throw(std::bad_alloc, std::runtime_error)
{
	if(!user_memory)
		throw std::runtime_error("Can't copy to non-user memory");
	if(this == &ls)
		return *this;
	if(allocated < static_cast<size_t>(ls.width) * ls.height) {
		size_t p_allocated = static_cast<size_t>(ls.width) * ls.height;
		memory = new uint32_t[p_allocated];
		allocated = p_allocated;
	}
	width = ls.width;
	height = ls.height;
	pitch = width;
	for(size_t l = 0; l < height; l++)
		memcpy(const_cast<uint32_t*>(memory + l * width), ls.memory + l * ls.pitch, 4 * width);
	return *this;
}

lcscreen::~lcscreen()
{
	if(user_memory)
		delete[] const_cast<uint32_t*>(memory);
}

void lcscreen::load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error)
{
	if(!user_memory)
		throw std::runtime_error("Can't load to non-user memory");
	const uint8_t* data2 = reinterpret_cast<const uint8_t*>(&data[0]);
	if(data.size() < 2)
		throw std::runtime_error("Corrupt saved screenshot data");
	uint32_t _width = static_cast<uint32_t>(data2[0]) * 256 + static_cast<uint32_t>(data2[1]);
	if(_width > 1 && data.size() % (3 * _width) != 2)
		throw std::runtime_error("Corrupt saved screenshot data");
	uint32_t _height = (data.size() - 2) / (3 * _width);
	if(allocated < static_cast<size_t>(_width) * _height) {
		size_t p_allocated = static_cast<size_t>(_width) * _height;
		memory = new uint32_t[p_allocated];
		allocated = p_allocated;
	}
	uint32_t* mem = const_cast<uint32_t*>(memory);
	width = _width;
	height = _height;
	pitch = width;
	for(size_t i = 0; i < (data.size() - 2) / 2; i++)
		mem[i] = static_cast<uint32_t>(data2[2 + 3 * i]) * 65536 +
			static_cast<uint32_t>(data2[2 + 3 * i + 1]) * 256 +
			static_cast<uint32_t>(data2[2 + 3 * i + 2]);
}

void lcscreen::save(std::vector<char>& data) throw(std::bad_alloc)
{
	data.resize(2 + 3 * static_cast<size_t>(width) * height);
	uint8_t* data2 = reinterpret_cast<uint8_t*>(&data[0]);
	data2[0] = (width >> 8);
	data2[1] = width;
	for(size_t i = 0; i < (data.size() - 2) / 3; i++) {
		data[2 + 3 * i] = memory[(i / width) * pitch + (i % width)] >> 16;
		data[2 + 3 * i + 1] = memory[(i / width) * pitch + (i % width)] >> 8;
		data[2 + 3 * i + 2] = memory[(i / width) * pitch + (i % width)];
	}
}

void lcscreen::save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	uint8_t* buffer = new uint8_t[3 * static_cast<size_t>(width) * height];
	for(uint32_t j = 0; j < height; j++)
		for(uint32_t i = 0; i < width; i++) {
			uint32_t word = memory[pitch * j + i];
			uint32_t l = (word >> 15) & 0xF;
			uint32_t r = l * ((word >> 0) & 0x1F);
			uint32_t g = l * ((word >> 5) & 0x1F);
			uint32_t b = l * ((word >> 10) & 0x1F);
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 0] = r * 255 / 465;
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 1] = g * 255 / 465;
			buffer[3 * static_cast<size_t>(width) * j + 3 * i + 2] = b * 255 / 465;
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
		const uint32_t* sbase = scr.memory + y * scr.pitch;
		for(uint32_t x = 0; x < copyable_width; x++) {
			uint32_t c = palette[sbase[x] & 0x7FFFF];
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
	memory = NULL;
	width = height = originx = originy = pitch = 0;
	user_memory = false;
	flipped = false;
	palette = NULL;
	set_palette(16, 8, 0);
}

screen::~screen() throw()
{
	if(memory && !user_memory)
		delete[] memory;
}

void clip_range(uint32_t origin, uint32_t size, int32_t base, int32_t& minc, int32_t& maxc) throw()
{
	int64_t _origin = origin;
	int64_t _size = size;
	int64_t _base = base;
	int64_t _minc = minc;
	int64_t _maxc = maxc;
	int64_t mincoordinate = _base + _origin + _minc;
	int64_t maxcoordinate = _base + _origin + _maxc;
	if(mincoordinate < 0)
		_minc = _minc - mincoordinate;
	if(maxcoordinate > _size)
		_maxc = _maxc - (maxcoordinate - _size);
	if(_minc >= maxc) {
		minc = 0;
		maxc = 0;
	} else {
		minc = _minc;
		maxc = _maxc;
	}
}

void screen::set_palette(uint32_t r, uint32_t g, uint32_t b)
{
	if(!palette)
		palette = new uint32_t[0x80000];
	else if(r == palette_r && g == palette_g && b == palette_b)
		return;
	for(size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
		uint32_t word = memory[i];
		uint32_t R = (word >> palette_r) & 0xFF;
		uint32_t G = (word >> palette_g) & 0xFF;
		uint32_t B = (word >> palette_b) & 0xFF;
		memory[i] = (R << r) | (G << g) | (B << b);
	}
	for(unsigned i = 0; i < 0x80000; i++) {
		unsigned l = (i >> 15) & 0xF;
		unsigned R = (i >> 0) & 0x1F;
		unsigned G = (i >> 5) & 0x1F;
		unsigned B = (i >> 10) & 0x1F;
		double _l = static_cast<double>(l);
		double m = 17.0 / 31.0;
		R = floor(m * R * _l + 0.5);
		G = floor(m * G * _l + 0.5);
		B = floor(m * B * _l + 0.5);
		palette[i] = (R << r) | (G << g) | (B << b);
	}
	palette_r = r;
	palette_g = g;
	palette_b = b;
}
