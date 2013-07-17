#ifndef _lua__bitmap__hpp__included__
#define _lua__bitmap__hpp__included__

#include <vector>
#include <string>
#include <cstdint>
#include "core/window.hpp"
#include "library/framebuffer.hpp"
#include "library/threadtypes.hpp"

struct lua_bitmap
{
	lua_bitmap(uint32_t w, uint32_t h);
	~lua_bitmap();
	size_t width;
	size_t height;
	std::vector<uint16_t> pixels;
};

struct lua_dbitmap
{
	lua_dbitmap(uint32_t w, uint32_t h);
	~lua_dbitmap();
	size_t width;
	size_t height;
	std::vector<premultiplied_color> pixels;
};

struct lua_palette
{
	std::vector<premultiplied_color> colors;
	lua_palette();
	~lua_palette();
	mutex_class palette_mutex;
};

struct lua_loaded_bitmap
{
	size_t w;
	size_t h;
	bool d;
	std::vector<int64_t> bitmap;
	std::vector<int64_t> palette;
	static struct lua_loaded_bitmap load(std::istream& stream);
	static struct lua_loaded_bitmap load(const std::string& name);
};


#endif
