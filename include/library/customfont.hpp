#ifndef _customfont__hpp__included__
#define _customfont__hpp__included__

#include <vector>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <map>
#include "framebuffer.hpp"

struct font_glyph_data
{
	font_glyph_data();
	font_glyph_data(std::istream& s);
	unsigned width;
	unsigned height;
	unsigned stride;
	std::vector<uint32_t> glyph;	//Bitpacked, element breaks between rows.
	void render(framebuffer<false>& fb, int32_t x, int32_t y, premultiplied_color fg, premultiplied_color bg)
		const;
	void render(framebuffer<true>& fb, int32_t x, int32_t y, premultiplied_color fg, premultiplied_color bg) const;
};

struct custom_font
{
public:
	custom_font();
	custom_font(const std::string& file);
	void add(const std::u32string& key, const font_glyph_data& glyph) throw(std::bad_alloc);
	std::u32string best_ligature_match(const std::u32string& codepoints, size_t start) const
		throw(std::bad_alloc);
	const font_glyph_data& lookup_glyph(const std::u32string& key) const throw();
	unsigned get_rowadvance() const throw() { return rowadvance; }
private:
	std::map<std::u32string, font_glyph_data> glyphs;
	unsigned rowadvance;
};

#endif