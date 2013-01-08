#ifndef _interface_cover__hpp__included__
#define _interface_cover__hpp__included__

#include <string>
#include <cstdlib>
#include <vector>

void cover_render_character(void* fb, unsigned x, unsigned y, uint32_t ch, uint32_t fg, uint32_t bg, size_t w,
	size_t h, size_t istride, size_t pstride);
void cover_render_string(void* fb, unsigned x, unsigned y, const std::string& str, uint32_t fg, uint32_t bg,
	size_t w, size_t h, size_t istride, size_t pstride);
void cover_next_position(uint32_t ch, unsigned& x, unsigned& y);
void cover_next_position(const std::string& ch, unsigned& x, unsigned& y);
std::vector<std::string> cover_information();

#endif
