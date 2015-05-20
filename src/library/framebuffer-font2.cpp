#include "framebuffer-font2.hpp"
#include "range.hpp"
#include "serialization.hpp"
#include <cstring>
#include "zip.hpp"
#include "string.hpp"

namespace framebuffer
{
namespace
{
	inline bool readfont(const font2::glyph& fglyph, uint32_t xp1, uint32_t yp1)
	{
		if(xp1 < 1 || xp1 > fglyph.width || yp1 < 1 || yp1 > fglyph.height)
			return false;
		xp1--;
		yp1--;
		size_t ge = yp1 * fglyph.stride + (xp1 / 32);
		size_t gb = 31 - xp1 % 32;
		return ((fglyph.fglyph[ge] >> gb) & 1);
	}

	template<bool T> void _render(const font2::glyph& fglyph, fb<T>& fb, int32_t x, int32_t y,
		color fg, color bg, color hl)
	{
		uint32_t _x = x;
		uint32_t _y = y;
		if(hl) {
			_x--;
			_y--;
			range bX = (range::make_w(fb.get_width()) - _x) & range::make_w(fglyph.width + 2);
			range bY = (range::make_w(fb.get_height()) - _y) & range::make_w(fglyph.height + 2);
			for(unsigned i = bY.low(); i < bY.high(); i++) {
				auto p = fb.rowptr(i + _y) + (_x + bX.low());
				for(unsigned j = bX.low(); j < bX.high(); j++) {
					bool in_halo = false;
					in_halo |= readfont(fglyph, j - 1, i - 1);
					in_halo |= readfont(fglyph, j,     i - 1);
					in_halo |= readfont(fglyph, j + 1, i - 1);
					in_halo |= readfont(fglyph, j - 1, i    );
					in_halo |= readfont(fglyph, j + 1, i    );
					in_halo |= readfont(fglyph, j - 1, i + 1);
					in_halo |= readfont(fglyph, j,     i + 1);
					in_halo |= readfont(fglyph, j + 1, i + 1);
					if(readfont(fglyph, j, i))
						fg.apply(p[j]);
					else if(in_halo)
						hl.apply(p[j]);
					else
						bg.apply(p[j]);

				}
			}
		} else {
			range bX = (range::make_w(fb.get_width()) - _x) & range::make_w(fglyph.width);
			range bY = (range::make_w(fb.get_height()) - _y) & range::make_w(fglyph.height);
			for(unsigned i = bY.low(); i < bY.high(); i++) {
				auto p = fb.rowptr(i + _y) + (_x + bX.low());
				for(unsigned j = bX.low(); j < bX.high(); j++) {
					size_t ge = i * fglyph.stride + (j / 32);
					size_t gb = 31 - j % 32;
					if((fglyph.fglyph[ge] >> gb) & 1)
						fg.apply(p[j]);
					else
						bg.apply(p[j]);
				}
			}
		}
	}
}

font2::glyph::glyph()
{
	stride = width = height = 0;
}

font2::glyph::glyph(std::istream& s)
{
	char header[40];
	bool old = true;
	bool upside_down = true;
	size_t rcount = 26;
	s.read(header, 26);
	if(!s)
		throw std::runtime_error("Can't read glyph bitmap header");
	if(serialization::u16l(header + 0) != 0x4D42)
		throw std::runtime_error("Bad glyph BMP magic");
	if(serialization::u16l(header + 14) != 12) {
		//Not OS/2 format.
		old = false;
		rcount = 40;
		s.read(header + 26, 14);
		if(!s)
			throw std::runtime_error("Can't read glyph bitmap header");
	}

	uint32_t startoff = serialization::u32l(header + 10);
	if(old) {
		width = serialization::u16l(header + 18);
		height = serialization::u16l(header + 20);
		if(serialization::u16l(header + 22) != 1)
			throw std::runtime_error("Bad glyph BMP planecount");
		if(serialization::u16l(header + 24) != 1)
			throw std::runtime_error("Bad glyph BMP bitdepth");
		if(startoff < 26)
			throw std::runtime_error("Glyph BMP data can't overlap header");
	} else {
		long _width = serialization::s32l(header + 18);
		long _height = serialization::s32l(header + 22);
		if(_width < 0)
			throw std::runtime_error("Bad glyph BMP size");
		if(_height < 0)
			upside_down = false;
		width = _width;
		height = (_height >= 0) ? height : -height;

		if(serialization::u16l(header + 26) != 1)
			throw std::runtime_error("Bad glyph BMP planecount");
		if(serialization::u16l(header + 28) != 1)
			throw std::runtime_error("Bad glyph BMP bitdepth");
		if(serialization::u32l(header + 30) != 0)
			throw std::runtime_error("Bad glyph BMP compression method");
		if(startoff < 40)
			throw std::runtime_error("Glyph BMP data can't overlap header");
	}
	//Discard data until start of bitmap.
	while(rcount < startoff) {
		s.get();
		if(!s)
			throw std::runtime_error("EOF while skipping to BMP data");
		rcount++;
	}
	stride = (width + 31) / 32;
	fglyph.resize(stride * height);
	memset(&fglyph[0], 0, sizeof(uint32_t) * fglyph.size());
	size_t toskip = (4 - ((width + 7) / 8) % 4) % 4;
	for(size_t i = 0; i < height; i++) {
		size_t y = upside_down ? (height - i - 1) : i;
		size_t bpos = y * stride * 32;
		for(size_t j = 0; j < width; j += 8) {
			size_t e = (bpos + j) / 32;
			size_t b = (bpos + j) % 32;
			int c = s.get();
			if(!s)
				throw std::runtime_error("EOF while reading BMP data");
			fglyph[e] |= ((uint32_t)c << (24 - b));
		}
		for(size_t j = 0; j < toskip; j++) {
			s.get();
			if(!s)
				throw std::runtime_error("EOF while reading BMP data");
		}
	}
}

void font2::glyph::render(fb<false>& fb, int32_t x, int32_t y, color fg,
	color bg, color hl) const
{
	_render(*this, fb, x, y, fg, bg, hl);
}

void font2::glyph::render(fb<true>& fb, int32_t x, int32_t y, color fg,
	color bg, color hl) const
{
	_render(*this, fb, x, y, fg, bg, hl);
}


font2::font2()
{
	rowadvance = 0;
}

font2::font2(const std::string& file)
{
	std::istream* toclose = NULL;
	rowadvance = 0;
	try {
		zip::reader r(file);
		for(auto member : r) {
			//Parse the key out of filename.
			std::u32string key;
			std::string tname = member;
			std::string tmp;
			if(tname == "bad") {
				//Special, no key.
			} else if(regex_match("[0-9]+(-[0-9]+)*", tname))
				for(auto& tmp : token_iterator<char>::foreach(tname, {"-"}))
					key.append(1, parse_value<uint32_t>(tmp));
			else {
				delete toclose;
				toclose = NULL;
				continue;
			}
			std::istream& s = r[member];
			toclose = &s;
			try {
				add(key, glyph(s));
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				throw std::runtime_error(tname + std::string(": ") + e.what());
			}
			delete toclose;
			toclose = NULL;
		}
	} catch(std::bad_alloc& e) {
		if(toclose)
			delete toclose;
		throw;
	} catch(std::exception& e) {
		if(toclose)
			delete toclose;
		throw std::runtime_error(std::string("Error reading font: ") + e.what());
	}
}

font2::font2(struct font& bfont)
{
	auto s = bfont.get_glyphs_set();
	for(auto i = s.begin();;i++) {
		const font::glyph& j = (i != s.end()) ? bfont.get_glyph(*i) : bfont.get_bad_glyph();
		glyph k;
		k.width = j.wide ? 16 : 8;
		k.height = 16;
		k.stride = 1;
		k.fglyph.resize(16);
		for(size_t y = 0; y < 16; y++) {
			k.fglyph[y] = 0;
			uint32_t r = j.data[y / (j.wide ? 2 : 4)];
			if(j.wide)
				r >>= 16 - ((y & 1) << 4);
			else
				r >>= 24 - ((y & 3) << 3);
			for(size_t x = 0; x < k.width; x++) {
				uint32_t b = (j.wide ? 15 : 7) - x;
				if(((r >> b) & 1) != 0)
					k.fglyph[y] |= 1UL << (31 - x);
			}
		}
		std::u32string key = (i != s.end()) ? std::u32string(1, *i) : std::u32string();
		glyphs[key] = k;
		if(i == s.end()) break;
	}
	rowadvance = 16;
}

std::ostream& operator<<(std::ostream& os, const std::u32string& lkey)
{
	if(!lkey.length())
		return (os << "bad");
	for(size_t i = 0; i < lkey.length(); i++) {
		if(i)
			os << "-";
		os << static_cast<uint32_t>(lkey[i]);
	}
	return os;
}

void font2::add(const std::u32string& key, const glyph& fglyph) throw(std::bad_alloc)
{
	glyphs[key] = fglyph;
	if(fglyph.height > rowadvance)
		rowadvance = fglyph.height;
}

std::u32string font2::best_ligature_match(const std::u32string& codepoints, size_t start) const
	throw(std::bad_alloc)
{
	std::u32string tmp;
	if(start >= codepoints.length())
		return tmp;		//Bad.
	std::u32string best = tmp;
	for(size_t i = 1; i <= codepoints.size() - start; i++) {
		tmp.append(1, codepoints[start + i - 1]);
		std::u32string lkey = tmp;
		if(glyphs.count(lkey))
			best = lkey;
		auto j = glyphs.lower_bound(lkey);
		//If lower_bound is greater than equivalent length of string, there can be no better match.
		if(j == glyphs.end())
			break;
		const std::u32string& tmp2 = j->first;
		bool best_found = false;
		for(size_t k = 0; k < tmp2.length() && start + k < codepoints.length(); k++)
			if(tmp2[k] > codepoints[start + k]) {
				best_found = true;
				break;
			} else if(tmp2[k] < codepoints[start + k])
				break;
		if(best_found)
			break;
	}
	return best;
}

const font2::glyph& font2::lookup_glyph(const std::u32string& key) const throw()
{
	static glyph empty_glyph;
	auto i = glyphs.find(key);
	return (i == glyphs.end()) ? empty_glyph : i->second;
}
}