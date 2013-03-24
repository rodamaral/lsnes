#include "customfont.hpp"
#include "serialization.hpp"
#include <cstring>
#include "zip.hpp"
#include "string.hpp"

namespace
{
	void bound(int32_t c, uint32_t odim, uint32_t dim, uint32_t& dc, uint32_t& off, uint32_t& size)
	{
		if(c >= (int32_t)dim || c + odim <= 0) {
			//Outside the screen.
			dc = 0;
			off = 0;
			size = 0;
		} else if(c >= 0) {
			dc = c;
			off = 0;
			size = odim;
		} else {
			dc = 0;
			off = -c;
			size = odim + c;
		}
		if(dc + size > dim)
			size = dim - dc;
	}

	template<bool T> void _render(const font_glyph_data& glyph, framebuffer<T>& fb, int32_t x, int32_t y,
		premultiplied_color fg, premultiplied_color bg)
	{
		uint32_t xdc, xoff, xsize;
		uint32_t ydc, yoff, ysize;
		bound(x, glyph.width, fb.get_width(), xdc, xoff, xsize);
		bound(y, glyph.height, fb.get_height(), ydc, yoff, ysize);
		if(!xsize || !ysize)
			return;
		for(unsigned i = 0; i < ysize; i++) {
			auto p = fb.rowptr(i + ydc);
			for(unsigned j = 0; j < xsize; j++) {
				size_t ge = (i + yoff) * glyph.stride + ((j + xoff) / 32);
				size_t gb = 31 - (j + xoff) % 32;
				if((glyph.glyph[ge] >> gb) & 1)
					fg.apply(p[j + xdc]);
				else
					bg.apply(p[j + xdc]);
			}
		}
	}
}

font_glyph_data::font_glyph_data()
{
	stride = width = height = 0;
}

font_glyph_data::font_glyph_data(std::istream& s)
{
	char header[40];
	bool old = true;
	bool upside_down = true;
	size_t rcount = 26;
	s.read(header, 26);
	if(!s)
		throw std::runtime_error("Can't read glyph bitmap header");
	if(read16ule(header + 0) != 0x4D42)
		throw std::runtime_error("Bad glyph BMP magic");
	if(read16ule(header + 14) != 12) {
		//Not OS/2 format.
		old = false;
		rcount = 40;
		s.read(header + 26, 14);
		if(!s)
			throw std::runtime_error("Can't read glyph bitmap header");
	}

	uint32_t startoff = read32ule(header + 10);
	if(old) {
		width = read16ule(header + 18);
		height = read16ule(header + 20);
		if(read16ule(header + 22) != 1)
			throw std::runtime_error("Bad glyph BMP planecount");
		if(read16ule(header + 24) != 1)
			throw std::runtime_error("Bad glyph BMP bitdepth");
		if(startoff < 26)
			throw std::runtime_error("Glyph BMP data can't overlap header");
	} else {
		long _width = read32sle(header + 18);
		long _height = read32sle(header + 22);
		if(_width < 0)
			throw std::runtime_error("Bad glyph BMP size");
		if(_height < 0)
			upside_down = false;
		width = _width;
		height = (_height >= 0) ? height : -height;

		if(read16ule(header + 26) != 1)
			throw std::runtime_error("Bad glyph BMP planecount");
		if(read16ule(header + 28) != 1)
			throw std::runtime_error("Bad glyph BMP bitdepth");
		if(read32ule(header + 30) != 0)
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
	glyph.resize(stride * height);
	memset(&glyph[0], 0, sizeof(uint32_t) * glyph.size());
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
			glyph[e] |= ((uint32_t)c << (24 - b));
		}
		for(size_t j = 0; j < toskip; j++) {
			s.get();
			if(!s)
				throw std::runtime_error("EOF while reading BMP data");
		}
	}
}

void font_glyph_data::render(framebuffer<false>& fb, int32_t x, int32_t y, premultiplied_color fg,
	premultiplied_color bg) const
{
	_render(*this, fb, x, y, fg, bg);
}

void font_glyph_data::render(framebuffer<true>& fb, int32_t x, int32_t y, premultiplied_color fg,
	premultiplied_color bg) const
{
	_render(*this, fb, x, y, fg, bg);
}


custom_font::custom_font()
{
	rowadvance = 0;
}

custom_font::custom_font(const std::string& file)
{
	std::istream* toclose = NULL;
	rowadvance = 0;
	try {
		zip_reader r(file);
		for(auto member : r) {
			//Parse the key out of filename.
			std::u32string key;
			std::string tname = member;
			std::string tmp;
			if(tname == "bad") {
				//Special, no key.
			} else if(regex_match("[0-9]+(-[0-9]+)*", tname))
				while(tname != "") {
					extract_token(tname, tmp, "-");
					key.append(1, parse_value<uint32_t>(tmp));
				}
			else {
				delete toclose;
				toclose = NULL;
				continue;
			}
			std::istream& s = r[member];
			toclose = &s;
			try {
				add(key, font_glyph_data(s));
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

void custom_font::add(const std::u32string& key, const font_glyph_data& glyph) throw(std::bad_alloc)
{
	glyphs[key] = glyph;
	if(glyph.height > rowadvance)
		rowadvance = glyph.height;
}

std::u32string custom_font::best_ligature_match(const std::u32string& codepoints, size_t start) const
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

const font_glyph_data& custom_font::lookup_glyph(const std::u32string& key) const throw()
{
	static font_glyph_data empty_glyph;
	auto i = glyphs.find(key);
	return (i == glyphs.end()) ? empty_glyph : i->second;
}
