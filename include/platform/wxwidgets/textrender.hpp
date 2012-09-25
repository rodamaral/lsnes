#ifndef _textrender__hpp__included__
#define _textrender__hpp__included__

#include <cstdint>
#include <map>
#include <cstdlib>
#include <vector>
#include <string>

struct text_framebuffer
{
	text_framebuffer();
	text_framebuffer(size_t w, size_t h);
	struct element
	{
		element() { ch = 32; bg = 0xFFFFFF; fg = 0; }
		uint32_t ch;
		uint32_t bg;
		uint32_t fg;
		const static uint32_t white;
		const static uint32_t black;
	};
	element* get_buffer() { return &buffer[0]; }
	void set_size(size_t _width, size_t _height);
	size_t get_stride() { return width; }
	void clear() { for(size_t i = 0; i < buffer.size(); i++) buffer[i] = element(); }
	std::pair<size_t, size_t> get_characters() { return std::make_pair(width, height); }
	std::pair<size_t, size_t> get_cell();
	std::pair<size_t, size_t> get_pixels();
	void render(char* buffer);
	size_t write(const std::string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg);
	static size_t text_width(const std::string& text);
private:
	std::vector<element> buffer;
	size_t width;
	size_t height;
};

#endif
