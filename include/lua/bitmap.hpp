#ifndef _lua__bitmap__hpp__included__
#define _lua__bitmap__hpp__included__

#include <vector>
#include <string>
#include <cstdint>
#include "core/window.hpp"
#include "library/lua-base.hpp"
#include "library/lua-class.hpp"
#include "library/lua-params.hpp"
#include "library/framebuffer.hpp"
#include "library/range.hpp"
#include "library/threads.hpp"
#include "library/string.hpp"

struct lua_palette
{
	std::vector<framebuffer::color> lcolors;
	framebuffer::color* colors;
	framebuffer::color* scolors;
	lua_palette(lua::state& L);
	size_t color_count;
	const static size_t reserved_colors = 32;
	static size_t overcommit() {
		return lua::overcommit_std_align + reserved_colors * sizeof(framebuffer::color);
	}
	~lua_palette();
	threads::lock palette_mutex;
	std::string print();
	static int create(lua::state& L, lua::parameters& P);
	static int load(lua::state& L, lua::parameters& P);
	static int load_str(lua::state& L, lua::parameters& P);
	int set(lua::state& L, lua::parameters& P);
	int get(lua::state& L, lua::parameters& P);
	int hash(lua::state& L, lua::parameters& P);
	int debug(lua::state& L, lua::parameters& P);
	int adjust_transparency(lua::state& L, lua::parameters& P);
	void adjust_palette_size(size_t newsize);
	void push_back(const framebuffer::color& c);
};

struct lua_bitmap
{
	lua_bitmap(lua::state& L, uint32_t w, uint32_t h);
	static size_t overcommit(uint32_t w, uint32_t h) {
		return lua::overcommit_std_align + sizeof(uint16_t) * (size_t)w * h;
	}
	~lua_bitmap();
	size_t width;
	size_t height;
	uint16_t* pixels;
	std::vector<char> save_png(const lua_palette& pal) const;
	std::string print();
	static int create(lua::state& L, lua::parameters& P);
	template<bool outside, bool clip> int draw(lua::state& L, lua::parameters& P);
	int pset(lua::state& L, lua::parameters& P);
	int pget(lua::state& L, lua::parameters& P);
	int size(lua::state& L, lua::parameters& P);
	int hflip(lua::state& L, lua::parameters& P);
	int vflip(lua::state& L, lua::parameters& P);
	int hash(lua::state& L, lua::parameters& P);
	template<bool scaled, bool porterduff> int blit(lua::state& L, lua::parameters& P);
	template<bool scaled> int blit_priority(lua::state& L, lua::parameters& P);
	int save_png(lua::state& L, lua::parameters& P);
	int _save_png(lua::state& L, lua::parameters& P, bool is_method);
	int sample_texture(lua::state& L, lua::parameters& P);
};

struct lua_dbitmap
{
	lua_dbitmap(lua::state& L, uint32_t w, uint32_t h);
	static size_t overcommit(uint32_t w, uint32_t h) {
		return lua::overcommit_std_align + sizeof(framebuffer::color) * (size_t)w * h;
	}
	~lua_dbitmap();
	size_t width;
	size_t height;
	framebuffer::color* pixels;
	std::vector<char> save_png() const;
	std::string print();
	static int create(lua::state& L, lua::parameters& P);
	template<bool outside, bool clip> int draw(lua::state& L, lua::parameters& P);
	int pset(lua::state& L, lua::parameters& P);
	int pget(lua::state& L, lua::parameters& P);
	int size(lua::state& L, lua::parameters& P);
	int hflip(lua::state& L, lua::parameters& P);
	int vflip(lua::state& L, lua::parameters& P);
	int hash(lua::state& L, lua::parameters& P);
	template<bool scaled, bool porterduff> int blit(lua::state& L, lua::parameters& P);
	int save_png(lua::state& L, lua::parameters& P);
	int adjust_transparency(lua::state& L, lua::parameters& P);
	int _save_png(lua::state& L, lua::parameters& P, bool is_method);
	int sample_texture(lua::state& L, lua::parameters& P);
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
	template<bool png> static int load(lua::state& L, lua::parameters& P);
	template<bool png> static int load_str(lua::state& L, lua::parameters& P);
};

template<bool T> class lua_bitmap_holder
{
public:
	lua_bitmap_holder(lua_bitmap& _b, lua_palette& _p) : b(_b), p(_p) {};
	size_t stride() { return b.width; }
	void lock()
	{
		p.palette_mutex.lock();
		palette = p.colors;
		pallim = p.color_count;
	}
	void unlock()
	{
		p.palette_mutex.unlock();
	}
	void draw(size_t bmpidx, typename framebuffer::fb<T>::element_t& target)
	{
		uint16_t i = b.pixels[bmpidx];
		if(i < pallim)
			palette[i].apply(target);
	}
private:
	lua_bitmap& b;
	lua_palette& p;
	framebuffer::color* palette;
	size_t pallim;
};

template<bool T> class lua_dbitmap_holder
{
public:
	lua_dbitmap_holder(lua_dbitmap& _d) : d(_d) {};
	size_t stride() { return d.width; }
	void lock() {}
	void unlock() {}
	void draw(size_t bmpidx, typename framebuffer::fb<T>::element_t& target)
	{
		d.pixels[bmpidx].apply(target);
	}
private:
	lua_dbitmap& d;
};


template<bool T, class B> void lua_bitmap_composite(struct framebuffer::fb<T>& scr, int32_t xp,
	int32_t yp, const range& X, const range& Y, const range& sX, const range& sY, bool outside, B bmp) throw()
{
	if(!X.size() || !Y.size()) return;
	size_t stride = bmp.stride();
	bmp.lock();

	for(uint32_t r = Y.low(); r != Y.high(); r++) {
		typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(yp + r);
		size_t eptr = xp + X.low();
		uint32_t xmin = X.low();
		bool cut = outside && sY.in(r);
		if(cut && sX.in(xmin)) {
			xmin = sX.high();
		eptr += (sX.high() - X.low());
		}
		for(uint32_t c = xmin; c < X.high(); c++, eptr++) {
			if(__builtin_expect(cut && c == sX.low(), 0)) {
				c += sX.size();
				eptr += sX.size();
			}
			bmp.draw(r * stride + c, rptr[eptr]);
		}
	}
	bmp.unlock();
}

#endif
