#ifndef _library__framebuffer_font2__hpp__included__
#define _library__framebuffer_font2__hpp__included__

#include <vector>
#include <cstdint>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <map>
#include "framebuffer.hpp"

namespace framebuffer
{
struct font2
{
public:
	struct glyph
	{
		glyph();
		glyph(std::istream& s);
		unsigned width;
		unsigned height;
		unsigned stride;
		std::vector<uint32_t> fglyph;	//Bitpacked, element breaks between rows.
		void render(fb<false>& fb, int32_t x, int32_t y, color fg, color bg, color hl) const;
		void render(fb<true>& fb, int32_t x, int32_t y, color fg, color bg, color hl) const;
		void render(uint8_t* buf, size_t stride, uint32_t u, uint32_t v, uint32_t w, uint32_t h) const;
	};
	font2();
	font2(const std::string& file);
	font2(struct font& bfont);
	void add(const std::u32string& key, const glyph& fglyph) throw(std::bad_alloc);
	std::u32string best_ligature_match(const std::u32string& codepoints, size_t start) const
		throw(std::bad_alloc);
	const glyph& lookup_glyph(const std::u32string& key) const throw();
	unsigned get_rowadvance() const throw() { return rowadvance; }
	std::pair<uint32_t, uint32_t>  get_metrics(const std::u32string& str, uint32_t xalign) const;
	void for_each_glyph(const std::u32string& str, uint32_t xalign, std::function<void(uint32_t x, uint32_t y,
		const glyph& g)> cb) const;
private:
	std::map<std::u32string, glyph> glyphs;
	unsigned rowadvance;
};
}
#endif
