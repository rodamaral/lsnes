#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/png-codec.hpp"
#include "library/sha256.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include "lua/bitmap.hpp"
#include "library/threadtypes.hpp"
#include <vector>
#include <sstream>

lua_bitmap::lua_bitmap(lua_state& L, uint32_t w, uint32_t h)
{
	width = w;
	height = h;
	pixels.resize(width * height);
	memset(&pixels[0], 0, width * height);
}

lua_bitmap::~lua_bitmap()
{
	render_kill_request(this);
}

std::string lua_bitmap::print()
{
	return (stringfmt() << width << "*" << height).str();
}

lua_dbitmap::lua_dbitmap(lua_state& L, uint32_t w, uint32_t h)
{
	width = w;
	height = h;
	pixels.resize(width * height);
}

lua_dbitmap::~lua_dbitmap()
{
	render_kill_request(this);
}

std::string lua_dbitmap::print()
{
	return (stringfmt() << width << "*" << height).str();
}

lua_palette::lua_palette(lua_state& L)
{
}

lua_palette::~lua_palette()
{
}

std::string lua_palette::print()
{
	size_t s = colors.size();
	return (stringfmt() << s << " " << ((s != 1) ? "colors" : "color")).str();
}

std::vector<char> lua_dbitmap::save_png() const
{
	png_encodedable_image img;
	img.width = width;
	img.height = height;
	img.has_palette = false;
	img.has_alpha = false;
	img.data.resize(width * height);
	for(size_t i = 0; i < width * height; i++) {
		const framebuffer::color& c = pixels[i];
		if(c.origa != 256)
			img.has_alpha = true;
		img.data[i] = c.orig + ((uint32_t)(c.origa - (c.origa >> 7) + (c.origa >> 8)) << 24);
	}
	std::ostringstream tmp1;
	img.encode(tmp1);
	std::string tmp2 = tmp1.str();
	return std::vector<char>(tmp2.begin(), tmp2.end());
}

std::vector<char> lua_bitmap::save_png(const lua_palette& pal) const
{
	png_encodedable_image img;
	img.width = width;
	img.height = height;
	img.has_palette = true;
	img.has_alpha = false;
	img.data.resize(width * height);
	img.palette.resize(pal.colors.size());
	for(size_t i = 0; i < width * height; i++) {
		img.data[i] = pixels[i];
	}
	for(size_t i = 0; i < pal.colors.size(); i++) {
		const framebuffer::color& c = pal.colors[i];
		if(c.origa != 256)
			img.has_alpha = true;
		img.palette[i] = c.orig + ((uint32_t)(c.origa - (c.origa >> 7) + (c.origa >> 8)) << 24);
	}
	std::ostringstream tmp1;
	img.encode(tmp1);
	std::string tmp2 = tmp1.str();
	return std::vector<char>(tmp2.begin(), tmp2.end());
}

namespace
{
	const char* base64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	struct render_object_bitmap : public framebuffer::object
	{
		render_object_bitmap(int32_t _x, int32_t _y, lua_obj_pin<lua_bitmap> _bitmap,
			lua_obj_pin<lua_palette> _palette) throw()
		{
			x = _x;
			y = _y;
			b = _bitmap;
			p = _palette;
		}

		render_object_bitmap(int32_t _x, int32_t _y, lua_obj_pin<lua_dbitmap> _bitmap) throw()
		{
			x = _x;
			y = _y;
			b2 = _bitmap;
		}

		~render_object_bitmap() throw()
		{
		}

		bool kill_request(void* obj) throw()
		{
				return kill_request_ifeq(p.object(), obj) ||
				kill_request_ifeq(b.object(), obj) ||
				kill_request_ifeq(b2.object(), obj);
		}

		template<bool T> void composite_op(struct framebuffer::fb<T>& scr) throw()
		{
			if(p)
				p->palette_mutex.lock();
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			size_t pallim = 0;
			size_t w, h;
			framebuffer::color* palette;
			if(b) {
				palette = &p->colors[0];
				for(auto& c : p->colors)
					c.set_palette(scr);
				pallim = p->colors.size();
				w = b->width;
				h = b->height;
			} else {
				for(auto& c : b2->pixels)
					c.set_palette(scr);
				w = b2->width;
				h = b2->height;
			}

			int32_t xmin = 0;
			int32_t xmax = w;
			int32_t ymin = 0;
			int32_t ymax = h;
			framebuffer::clip_range(originx, scr.get_width(), x, xmin, xmax);
			framebuffer::clip_range(originy, scr.get_height(), y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(y + r + originy);
				size_t eptr = x + xmin + originx;
				if(b)
					for(int32_t c = xmin; c < xmax; c++, eptr++) {
						uint16_t i = b->pixels[r * b->width + c];
						if(i < pallim)
							palette[i].apply(rptr[eptr]);
					}
				else
					for(int32_t c = xmin; c < xmax; c++, eptr++)
						b2->pixels[r * b2->width + c].apply(rptr[eptr]);
			}
			if(p)
				p->palette_mutex.unlock();
		}
		void operator()(struct framebuffer::fb<false>& x) throw() { composite_op(x); }
		void operator()(struct framebuffer::fb<true>& x) throw() { composite_op(x); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		lua_obj_pin<lua_bitmap> b;
		lua_obj_pin<lua_dbitmap> b2;
		lua_obj_pin<lua_palette> p;
	};

	function_ptr_luafun gui_bitmap(lua_func_misc, "gui.bitmap_draw", [](lua_state& L, const std::string& fname)
		-> int {
		if(!lua_render_ctx)
			return 0;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		if(lua_class<lua_bitmap>::is(L, 3)) {
			lua_class<lua_bitmap>::get(L, 3, fname.c_str());
			lua_class<lua_palette>::get(L, 4, fname.c_str());
			auto b = lua_class<lua_bitmap>::pin(L, 3, fname.c_str());
			auto p = lua_class<lua_palette>::pin(L, 4, fname.c_str());
			lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b, p);
		} else if(lua_class<lua_dbitmap>::is(L, 3)) {
			lua_class<lua_dbitmap>::get(L, 3, fname.c_str());
			auto b = lua_class<lua_dbitmap>::pin(L, 3, fname.c_str());
			lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b);
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 3 for gui.bitmap_draw.");
		return 0;
	});

	function_ptr_luafun gui_cpalette(lua_func_misc, "gui.palette_new", [](lua_state& L, const std::string& fname)
		-> int {
		lua_class<lua_palette>::create(L);
		return 1;
	});

	function_ptr_luafun gui_cbitmap(lua_func_misc, "gui.bitmap_new", [](lua_state& L, const std::string& fname)
		-> int {
		uint32_t w = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		uint32_t h = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		bool d = L.get_bool(3, fname.c_str());
		if(d) {
			int64_t c = -1;
			L.get_numeric_argument<int64_t>(4, c, fname.c_str());
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = framebuffer::color(c);
		} else {
			uint16_t c = 0;
			L.get_numeric_argument<uint16_t>(4, c, fname.c_str());
			lua_bitmap* b = lua_class<lua_bitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = c;
		}
		return 1;
	});

	function_ptr_luafun gui_epalette(lua_func_misc, "gui.palette_set", [](lua_state& L, const std::string& fname)
		-> int {
		lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
		uint16_t c = L.get_numeric_argument<uint16_t>(2, fname.c_str());
		int64_t nval = L.get_numeric_argument<int64_t>(3, fname.c_str());
		framebuffer::color nc(nval);
		//The mutex lock protects only the internals of colors array.
		if(p->colors.size() <= c) {
			p->palette_mutex.lock();
			p->colors.resize(static_cast<uint32_t>(c) + 1);
			p->palette_mutex.unlock();
		}
		p->colors[c] = nc;
		return 0;
	});

	function_ptr_luafun pset_bitmap(lua_func_misc, "gui.bitmap_pset", [](lua_state& L, const std::string& fname)
		-> int {
		uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		if(lua_class<lua_bitmap>::is(L, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			uint16_t c = L.get_numeric_argument<uint16_t>(4, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			b->pixels[y * b->width + x] = c;
		} else if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			int64_t c = L.get_numeric_argument<int64_t>(4, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			b->pixels[y * b->width + x] = framebuffer::color(c);
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_pset.");
		return 0;
	});

	inline int64_t demultiply_color(const framebuffer::color& c)
	{
		if(!c.origa)
			return -1;
		else
			return c.orig | ((uint32_t)(256 - c.origa) << 24);
	}

	function_ptr_luafun pget_bitmap(lua_func_misc, "gui.bitmap_pget", [](lua_state& L, const std::string& fname)
		-> int {
		uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		if(lua_class<lua_bitmap>::is(L, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			L.pushnumber(b->pixels[y * b->width + x]);
		} else if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			L.pushnumber(demultiply_color(b->pixels[y * b->width + x]));
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_pget.");
		return 1;
	});

	function_ptr_luafun size_bitmap(lua_func_misc, "gui.bitmap_size", [](lua_state& L, const std::string& fname)
		-> int {
		if(lua_class<lua_bitmap>::is(L, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			L.pushnumber(b->width);
			L.pushnumber(b->height);
		} else if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			L.pushnumber(b->width);
			L.pushnumber(b->height);
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_size.");
		return 2;
	});

	function_ptr_luafun hash_bitmap(lua_func_misc, "gui.bitmap_hash", [](lua_state& L, const std::string& fname)
		-> int {
		sha256 h;
		const int buffersize = 256;
		int bufferuse = 0;
		char buf[buffersize];
		memset(buf, 0, buffersize);
		if(lua_class<lua_bitmap>::is(L, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			serialization::u64b(buf + 0, b->width);
			serialization::u64b(buf + 8, b->height);
			bufferuse = 16;
			for(unsigned i = 0; i < b->width * b->height; i++) {
				if(bufferuse + 2 > buffersize) {
					h.write(buf, bufferuse);
					bufferuse = 0;
				}
				serialization::u16b(buf + bufferuse + 0, b->pixels[i]);
				bufferuse += 2;
			}
			if(bufferuse > 0) h.write(buf, bufferuse);
			L.pushlstring(h.read());
			return 1;
		} else if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			serialization::u64b(buf + 0, b->width);
			serialization::u64b(buf + 4, b->height);
			bufferuse = 16;
			for(unsigned i = 0; i < b->width * b->height; i++) {
				if(bufferuse + 6 > buffersize) {
					h.write(buf, bufferuse);
					bufferuse = 0;
				}
				serialization::u32b(buf + bufferuse + 0, b->pixels[i].orig);
				serialization::u16b(buf + bufferuse + 4, b->pixels[i].origa);
				bufferuse += 6;
			}
			if(bufferuse > 0) h.write(buf, bufferuse);
			L.pushlstring(h.read());
			return 1;
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_hash.");
	});

	function_ptr_luafun hash_palette(lua_func_misc, "gui.palette_hash", [](lua_state& L, const std::string& fname)
		-> int {
		lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
		sha256 h;
		const int buffersize = 256;
		int bufferuse = 0;
		char buf[buffersize];
		unsigned realsize = 0;
		for(unsigned i = 0; i < p->colors.size(); i++)
			if(p->colors[i].origa) realsize = i + 1;
		for(unsigned i = 0; i < realsize; i++) {
			if(bufferuse + 6 > buffersize) {
				h.write(buf, bufferuse);
				bufferuse = 0;
			}
			serialization::u32b(buf + bufferuse + 0, p->colors[i].orig);
			serialization::u16b(buf + bufferuse + 4, p->colors[i].origa);
			bufferuse += 6;
		}
		if(bufferuse > 0) h.write(buf, bufferuse);
		L.pushlstring(h.read());
		return 1;
	});

	struct colorkey_none
	{
		bool iskey(uint16_t& c) const { return false; }
		bool iskey(framebuffer::color& c) const { return false; }
	};

	struct colorkey_direct
	{
		colorkey_direct(uint64_t _ck)
		{
			framebuffer::color c(_ck);
			ck = c.orig;
			cka = c.origa;
		}
		bool iskey(framebuffer::color& c) const { return (c.orig == ck && c.origa == cka); }
		uint32_t ck;
		uint16_t cka;
	};

	struct colorkey_palette
	{
		colorkey_palette(uint64_t _ck) { ck = _ck; }
		bool iskey(uint16_t& c) const { return (c == ck); }
		uint16_t ck;
	};

	template<class colorkey> struct srcdest_direct
	{
		srcdest_direct(lua_dbitmap& dest, lua_dbitmap& src, const colorkey& _ckey)
			: ckey(_ckey)
		{
			darray = &dest.pixels[0];
			sarray = &src.pixels[0];
			swidth = src.width;
			sheight = src.height;
			dwidth = dest.width;
			dheight = dest.height;
		}
		void copy(size_t didx, size_t sidx)
		{
			framebuffer::color c = sarray[sidx];
			if(!ckey.iskey(c))
				darray[didx] = c;
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		framebuffer::color* sarray;
		framebuffer::color* darray;
		const colorkey& ckey;
	};

	template<class colorkey> struct srcdest_palette
	{
		srcdest_palette(lua_bitmap& dest, lua_bitmap& src, const colorkey& _ckey)
			: ckey(_ckey)
		{
			darray = &dest.pixels[0];
			sarray = &src.pixels[0];
			swidth = src.width;
			sheight = src.height;
			dwidth = dest.width;
			dheight = dest.height;
		}
		void copy(size_t didx, size_t sidx)
		{
			uint16_t c = sarray[sidx];
			if(!ckey.iskey(c))
				darray[didx] = c;
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		uint16_t* sarray;
		uint16_t* darray;
		const colorkey& ckey;
	};

	template<class colorkey> struct srcdest_paletted
	{
		typedef framebuffer::color ptype;
		srcdest_paletted(lua_dbitmap& dest, lua_bitmap& src, lua_palette& palette, const colorkey& _ckey)
			: ckey(_ckey), transparent(-1)
		{
			darray = &dest.pixels[0];
			sarray = &src.pixels[0];
			limit = palette.colors.size();
			pal = &palette.colors[0];
			swidth = src.width;
			sheight = src.height;
			dwidth = dest.width;
			dheight = dest.height;
		}
		void copy(size_t didx, size_t sidx)
		{
			uint16_t c = sarray[sidx];
			if(!ckey.iskey(c))
				darray[didx] = (c < limit) ? pal[c] : transparent;
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		uint16_t* sarray;
		framebuffer::color* darray;
		framebuffer::color* pal;
		uint32_t limit;
		framebuffer::color transparent;
		const colorkey& ckey;
	};

	template<class srcdest>
	void blit(srcdest sd, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h)
	{
		while((dx + w > sd.dwidth || sx + w > sd.swidth) && w > 0)
			w--;
		while((dy + h > sd.dheight || sy + h > sd.sheight) && h > 0)
			h--;
		size_t sidx = sy * sd.swidth + sx;
		size_t didx = dy * sd.dwidth + dx;
		size_t srskip = sd.swidth - w;
		size_t drskip = sd.dwidth - w;
		for(uint32_t j = 0; j < h; j++) {
			for(uint32_t i = 0; i < w; i++) {
				sd.copy(didx, sidx);
				sidx++;
				didx++;
			}
			sidx += srskip;
			didx += drskip;
		}
	}

	function_ptr_luafun blit_bitmap(lua_func_misc, "gui.bitmap_blit", [](lua_state& L, const std::string& fname)
		-> int {
		int slot = 1;
		int dsts = 0;
		int srcs = 0;
		bool dst_d = lua_class<lua_dbitmap>::is(L, dsts = slot);
		bool dst_p = lua_class<lua_bitmap>::is(L, slot++);
		if(!dst_d && !dst_p)
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_blit");
		uint32_t dx = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		uint32_t dy = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		bool src_d = lua_class<lua_dbitmap>::is(L, srcs = slot);
		bool src_p = lua_class<lua_bitmap>::is(L, slot++);
		if(!src_d && !src_p)
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 4 for gui.bitmap_blit");
		if(dst_d && src_p)
			slot++;		//Reserve slot 5 for palette.
		uint32_t sx = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		uint32_t sy = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		uint32_t w = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		uint32_t h = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
		int64_t ck = 0x100000000ULL;
		L.get_numeric_argument<int64_t>(slot++, ck, fname.c_str());

		if(dst_d && src_d) {
			lua_dbitmap* db = lua_class<lua_dbitmap>::get(L, dsts, fname.c_str());
			lua_dbitmap* sb = lua_class<lua_dbitmap>::get(L, srcs, fname.c_str());
			if(ck == 0x100000000ULL)
				blit(srcdest_direct<colorkey_none>(*db, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
			else
				blit(srcdest_direct<colorkey_direct>(*db, *sb, colorkey_direct(ck)), dx, dy, sx, sy, w,
					h);
		} else if(dst_p && src_p) {
			lua_bitmap* db = lua_class<lua_bitmap>::get(L, dsts, fname.c_str());
			lua_bitmap* sb = lua_class<lua_bitmap>::get(L, srcs, fname.c_str());
			if(ck > 65535)
				blit(srcdest_palette<colorkey_none>(*db, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
			else
				blit(srcdest_palette<colorkey_palette>(*db, *sb, colorkey_palette(ck)), dx, dy, sx, sy,
					w, h);
		} else if(dst_d && src_p) {
			lua_dbitmap* db = lua_class<lua_dbitmap>::get(L, dsts, fname.c_str());
			lua_bitmap* sb = lua_class<lua_bitmap>::get(L, srcs, fname.c_str());
			lua_palette* pal = lua_class<lua_palette>::get(L, srcs + 1, fname.c_str());
			if(ck > 65535)
				blit(srcdest_paletted<colorkey_none>(*db, *sb, *pal, colorkey_none()), dx, dy, sx, sy,
					w, h);
			else
				blit(srcdest_paletted<colorkey_palette>(*db, *sb, *pal, colorkey_palette(ck)), dx, dy,
					sx, sy, w, h);
		} else
			throw std::runtime_error("If parameter 1 to gui.bitmap_blit is paletted, parameter 4 must be "
				"too");
		return 0;
	});

	int bitmap_load_fn(lua_state& L, std::function<lua_loaded_bitmap()> src)
	{
		uint32_t w, h;
		auto bitmap = src();
		if(bitmap.d) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, bitmap.w, bitmap.h);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = framebuffer::color(bitmap.bitmap[i]);
			return 1;
		} else {
			lua_bitmap* b = lua_class<lua_bitmap>::create(L, bitmap.w, bitmap.h);
			lua_palette* p = lua_class<lua_palette>::create(L);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = bitmap.bitmap[i];
			p->colors.resize(bitmap.palette.size());
			for(size_t i = 0; i < bitmap.palette.size(); i++)
				p->colors[i] = framebuffer::color(bitmap.palette[i]);
			return 2;
		}
	}

	function_ptr_luafun gui_loadbitmap(lua_func_misc, "gui.bitmap_load", [](lua_state& L,
		const std::string& fname) -> int {
		std::string name2;
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			name2 = L.get_string(2, fname.c_str());
		return bitmap_load_fn(L, [&name, &name2]() -> lua_loaded_bitmap {
			std::string name3 = zip::resolverel(name, name2);
			return lua_loaded_bitmap::load(name3);
		});
	});

	function_ptr_luafun gui_loadbitmap2(lua_func_misc, "gui.bitmap_load_str", [](lua_state& L,
		const std::string& fname) -> int {
		std::string contents = L.get_string(1, fname.c_str());
		return bitmap_load_fn(L, [&contents]() -> lua_loaded_bitmap {
			std::istringstream strm(contents);
			return lua_loaded_bitmap::load(strm);
		});
	});

	inline int64_t mangle_color(uint32_t c)
	{
		if(c < 0x1000000)
			return -1;
		else
			return ((256 - (c >> 24) - (c >> 31)) << 24) | (c & 0xFFFFFF);
	}

	int base64val(char ch)
	{
		if(ch >= 'A' && ch <= 'Z')
			return ch - 65;
		if(ch >= 'a' && ch <= 'z')
			return ch - 97 + 26;
		if(ch >= '0' && ch <= '9')
			return ch - 48 + 52;
		if(ch == '+')
			return 62;
		if(ch == '/')
			return 63;
		if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
			return -1;
		if(ch == '=')
			return -2;
		return -3;
	}

	std::string base64_encode(const std::string& str)
	{
		std::ostringstream x;
		unsigned pos = 0;
		uint32_t mem = 0;
		for(auto i : str) {
			mem = (mem << 8) + (unsigned char)i;
			if(++pos == 3) {
				uint8_t c1 = (mem >> 18) & 0x3F;
				uint8_t c2 = (mem >> 12) & 0x3F;
				uint8_t c3 = (mem >> 6) & 0x3F;
				uint8_t c4 = mem & 0x3F;
				x << base64chars[c1];
				x << base64chars[c2];
				x << base64chars[c3];
				x << base64chars[c4];
				mem = 0;
				pos = 0;
			}
		}
		if(pos == 2) {
			uint8_t c1 = (mem >> 10) & 0x3F;
			uint8_t c2 = (mem >> 4) & 0x3F;
			uint8_t c3 = (mem << 2) & 0x3F;
			x << base64chars[c1];
			x << base64chars[c2];
			x << base64chars[c3];
			x << "=";
		}
		if(pos == 1) {
			uint8_t c1 = (mem >> 2) & 0x3F;
			uint8_t c2 = (mem << 4) & 0x3F;
			x << base64chars[c1];
			x << base64chars[c2];
			x << "==";
		}
		return x.str();
	}

	std::string base64_decode(const std::string& str)
	{
		bool end = 0;
		uint32_t memory = 0;
		uint32_t memsize = 1;
		int posmod = 0;
		std::ostringstream x;
		for(auto i : str) {
			int v = base64val(i);
			if(v == -1)
				continue;
			posmod = (posmod + 1) & 3;
			if(v == -2 && (posmod == 1 || posmod == 2))
				throw std::runtime_error("Invalid Base64");
			if(v == -2) {
				end = true;
				continue;
			}
			if(v == -3 || end)
				throw std::runtime_error("Invalid Base64");
			memory = memory * 64 + v;
			memsize = memsize * 64;
			if(memsize >= 256) {
				memsize >>= 8;
				x << static_cast<uint8_t>(memory / memsize);
				memory %= memsize;
			}
		}
		return x.str();
	}

	template<typename T>
	int bitmap_load_png_fn(lua_state& L, T& src)
	{
		png_decoded_image img(src);
		if(img.has_palette) {
			lua_bitmap* b = lua_class<lua_bitmap>::create(L, img.width, img.height);
			lua_palette* p = lua_class<lua_palette>::create(L);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = img.data[i];
			p->colors.resize(img.palette.size());
			for(size_t i = 0; i < img.palette.size(); i++)
				p->colors[i] = framebuffer::color(mangle_color(img.palette[i]));
			return 2;
		} else {
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, img.width, img.height);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = framebuffer::color(mangle_color(img.data[i]));
			return 1;
		}
	}

	void bitmap_save_png_fn(lua_state& L, std::function<void(const std::vector<char>& buf)> fn, int index,
		const std::string& fname)
	{
		std::vector<char> buf;
		if(lua_class<lua_bitmap>::is(L, index)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, index, fname.c_str());
			lua_palette* p = lua_class<lua_palette>::get(L, index + 1, fname.c_str());
			buf = b->save_png(*p);
		} else if(lua_class<lua_dbitmap>::is(L, index)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, index, fname.c_str());
			buf = b->save_png();
		} else
			(stringfmt() << "Expected BITMAP or DBITMAP as argument " << index
				<< " for gui.bitmap_save_png.").throwex();
		fn(buf);
	}

	
	function_ptr_luafun gui_loadbitmappng(lua_func_misc, "gui.bitmap_load_png", [](lua_state& L,
		const std::string& fname) -> int {
		std::string name2;
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			name2 = L.get_string(2, fname.c_str());
		std::string filename = zip::resolverel(name, name2);
		return bitmap_load_png_fn(L, filename);
	});

	function_ptr_luafun gui_loadbitmappng2(lua_func_misc, "gui.bitmap_load_png_str", [](lua_state& L,
		const std::string& fname) -> int {
		std::string contents = base64_decode(L.get_string(1, fname.c_str()));
		std::istringstream strm(contents);
		return bitmap_load_png_fn(L, strm);
	});

	function_ptr_luafun gui_savebitmappng(lua_func_misc, "gui.bitmap_save_png", [](lua_state& L,
		const std::string& fname) -> int {
		int index = 1;
		std::string name, name2;
		if(L.type(index) == LUA_TSTRING) {
			name = L.get_string(index, fname.c_str());
			index++;
		}
		if(L.type(index) == LUA_TSTRING) {
			name2 = L.get_string(index, fname.c_str());
			index++;
		}
		if(index > 1) {
			std::string filename = zip::resolverel(name, name2);
			std::ofstream strm(filename, std::ios::binary);
			if(!strm)
				throw std::runtime_error("Can't open output file");
			bitmap_save_png_fn(L, [&strm](const std::vector<char>& x) { strm.write(&x[0], x.size()); },
				index, fname);
			if(!strm)
				throw std::runtime_error("Can't write output file");
			return 0;
		} else {
			std::ostringstream strm;
			bitmap_save_png_fn(L, [&strm](const std::vector<char>& x) { strm.write(&x[0], x.size()); }, 1,
				fname);
			L.pushlstring(base64_encode(strm.str()));
			return 1;
		}
	});

	int bitmap_palette_fn(lua_state& L, std::istream& s)
	{
		lua_palette* p = lua_class<lua_palette>::create(L);
		while(s) {
			std::string line;
			std::getline(s, line);
			istrip_CR(line);
			regex_results r;
			if(r = regex("[ \t]*([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]*", line)) {
				int64_t cr, cg, cb, ca;
				cr = parse_value<uint8_t>(r[1]);
				cg = parse_value<uint8_t>(r[2]);
				cb = parse_value<uint8_t>(r[3]);
				ca = 256 - parse_value<uint16_t>(r[4]);
				int64_t clr;
				if(ca == 256)
					p->colors.push_back(framebuffer::color(-1));
				else
					p->colors.push_back(framebuffer::color((ca << 24) | (cr << 16)
						| (cg << 8) | cb));
			} else if(r = regex("[ \t]*([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]*", line)) {
				int64_t cr, cg, cb;
				cr = parse_value<uint8_t>(r[1]);
				cg = parse_value<uint8_t>(r[2]);
				cb = parse_value<uint8_t>(r[3]);
				p->colors.push_back(framebuffer::color((cr << 16) | (cg << 8) | cb));
			} else if(regex_match("[ \t]*transparent[ \t]*", line)) {
				p->colors.push_back(framebuffer::color(-1));
			} else if(!regex_match("[ \t]*(#.*)?", line))
				throw std::runtime_error("Invalid line format (" + line + ")");
		}
		return 1;
	}

	function_ptr_luafun gui_loadpalette(lua_func_misc, "gui.bitmap_load_pal", [](lua_state& L,
		const std::string& fname) -> int {
		std::string name2;
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			name2 = L.get_string(2, fname.c_str());
		std::istream& s = zip::openrel(name, name2);
		try {
			int r = bitmap_palette_fn(L, s);
			delete &s;
			return r;
		} catch(...) {
			delete &s;
			throw;
		}
	});

	function_ptr_luafun gui_loadpalette2(lua_func_misc, "gui.bitmap_load_pal_str", [](lua_state& L,
		const std::string& fname) -> int {
		std::string content = L.get_string(1, fname.c_str());
		std::istringstream s(content);
		return bitmap_palette_fn(L, s);
	});

	function_ptr_luafun gui_dpalette(lua_func_misc, "gui.palette_debug", [](lua_state& L,
		const std::string& fname) -> int {
		lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
		size_t i = 0;
		for(auto c : p->colors)
			messages << "Color #" << (i++) << ": " << c.orig << ":" << c.origa << std::endl;
		return 0;
	});

	inline framebuffer::color tadjust(framebuffer::color c, uint16_t adj)
	{
		uint32_t rgb = c.orig;
		uint32_t a = c.origa;
		a = (a * adj) >> 8;
		if(a > 256)
			a = 256;
		if(a == 0)
			return framebuffer::color(-1);
		else
			return framebuffer::color(rgb | ((uint32_t)(256 - a) << 24));
	}

	function_ptr_luafun adjust_trans(lua_func_misc, "gui.adjust_transparency", [](lua_state& L,
		const std::string& fname) -> int {
		uint16_t tadj = L.get_numeric_argument<uint16_t>(2, fname.c_str());
		if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			for(auto& c : b->pixels)
				c = tadjust(c, tadj);
		} else if(lua_class<lua_palette>::is(L, 1)) {
			lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
			for(auto& c : p->colors)
				c = tadjust(c, tadj);
		} else {
			throw std::runtime_error("Expected BITMAP or PALETTE as argument 1 for "
				"gui.adjust_transparency");
		}
		return 2;
	});
}

DECLARE_LUACLASS(lua_palette, "PALETTE");
DECLARE_LUACLASS(lua_bitmap, "BITMAP");
DECLARE_LUACLASS(lua_dbitmap, "DBITMAP");
