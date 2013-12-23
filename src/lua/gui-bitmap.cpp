#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/png.hpp"
#include "library/sha256.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include "lua/bitmap.hpp"
#include "library/threadtypes.hpp"
#include <vector>
#include <sstream>

std::vector<char> lua_dbitmap::save_png() const
{
	png::encoder img;
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
	png::encoder img;
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
		render_object_bitmap(int32_t _x, int32_t _y, lua::objpin<lua_bitmap> _bitmap,
			lua::objpin<lua_palette> _palette) throw()
		{
			x = _x;
			y = _y;
			b = _bitmap;
			p = _palette;
		}

		render_object_bitmap(int32_t _x, int32_t _y, lua::objpin<lua_dbitmap> _bitmap) throw()
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
		lua::objpin<lua_bitmap> b;
		lua::objpin<lua_dbitmap> b2;
		lua::objpin<lua_palette> p;
	};

	lua::fnptr gui_bitmap(lua_func_misc, "gui.bitmap_draw", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 3))
			return lua::_class<lua_bitmap>::get(L, 3, fname.c_str())->_draw(L, fname, false);
		else if(lua::_class<lua_dbitmap>::is(L, 3))
			return lua::_class<lua_dbitmap>::get(L, 3, fname.c_str())->_draw(L, fname, false);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 3 for " + fname);
		return 0;
	});

	lua::fnptr gui_cpalette(lua_func_misc, "gui.palette_new", [](lua::state& L, const std::string& fname)
		-> int {
		lua::_class<lua_palette>::create(L);
		return 1;
	});

	lua::fnptr gui_cbitmap(lua_func_misc, "gui.bitmap_new", [](lua::state& L, const std::string& fname)
		-> int {
		uint32_t w = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		uint32_t h = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		bool d = L.get_bool(3, fname.c_str());
		if(d) {
			int64_t c = -1;
			L.get_numeric_argument<int64_t>(4, c, fname.c_str());
			lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = framebuffer::color(c);
		} else {
			uint16_t c = 0;
			L.get_numeric_argument<uint16_t>(4, c, fname.c_str());
			lua_bitmap* b = lua::_class<lua_bitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = c;
		}
		return 1;
	});

	lua::fnptr gui_epalette(lua_func_misc, "gui.palette_set", [](lua::state& L, const std::string& fname)
		-> int {
		lua_palette* p = lua::_class<lua_palette>::get(L, 1, fname.c_str());
		return p->set(L, fname);
	});

	lua::fnptr pset_bitmap(lua_func_misc, "gui.bitmap_pset", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 1))
			return lua::_class<lua_bitmap>::get(L, 1, fname.c_str())->pset(L, fname);
		else if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->pset(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for " + fname);
		return 0;
	});

	inline int64_t demultiply_color(const framebuffer::color& c)
	{
		if(!c.origa)
			return -1;
		else
			return c.orig | ((uint32_t)(256 - c.origa) << 24);
	}

	lua::fnptr pget_bitmap(lua_func_misc, "gui.bitmap_pget", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 1))
			return lua::_class<lua_bitmap>::get(L, 1, fname.c_str())->pget(L, fname);
		else if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->pget(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for " + fname);
		return 0;
	});

	lua::fnptr size_bitmap(lua_func_misc, "gui.bitmap_size", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 1))
			return lua::_class<lua_bitmap>::get(L, 1, fname.c_str())->size(L, fname);
		else if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->size(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for " + fname);
		return 0;
	});

	lua::fnptr hash_bitmap(lua_func_misc, "gui.bitmap_hash", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 1))
			return lua::_class<lua_bitmap>::get(L, 1, fname.c_str())->hash(L, fname);
		else if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->hash(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for " + fname);
		return 0;
	});

	lua::fnptr hash_palette(lua_func_misc, "gui.palette_hash", [](lua::state& L, const std::string& fname)
		-> int {
		lua_palette* p = lua::_class<lua_palette>::get(L, 1, fname.c_str());
		return p->hash(L, fname);
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
	void xblit(srcdest sd, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h)
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

	lua::fnptr blit_bitmap(lua_func_misc, "gui.bitmap_blit", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_bitmap>::is(L, 1))
			return lua::_class<lua_bitmap>::get(L, 1, fname.c_str())->blit(L, fname);
		else if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->blit(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for " + fname);
		return 0;
	});

	int bitmap_load_fn(lua::state& L, std::function<lua_loaded_bitmap()> src)
	{
		uint32_t w, h;
		auto bitmap = src();
		if(bitmap.d) {
			lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, bitmap.w, bitmap.h);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = framebuffer::color(bitmap.bitmap[i]);
			return 1;
		} else {
			lua_bitmap* b = lua::_class<lua_bitmap>::create(L, bitmap.w, bitmap.h);
			lua_palette* p = lua::_class<lua_palette>::create(L);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = bitmap.bitmap[i];
			p->colors.resize(bitmap.palette.size());
			for(size_t i = 0; i < bitmap.palette.size(); i++)
				p->colors[i] = framebuffer::color(bitmap.palette[i]);
			return 2;
		}
	}

	lua::fnptr gui_loadbitmap(lua_func_misc, "gui.bitmap_load", [](lua::state& L,
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

	lua::fnptr gui_loadbitmap2(lua_func_misc, "gui.bitmap_load_str", [](lua::state& L,
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
	int bitmap_load_png_fn(lua::state& L, T& src)
	{
		png::decoder img(src);
		if(img.has_palette) {
			lua_bitmap* b = lua::_class<lua_bitmap>::create(L, img.width, img.height);
			lua_palette* p = lua::_class<lua_palette>::create(L);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = img.data[i];
			p->colors.resize(img.palette.size());
			for(size_t i = 0; i < img.palette.size(); i++)
				p->colors[i] = framebuffer::color(mangle_color(img.palette[i]));
			return 2;
		} else {
			lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, img.width, img.height);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = framebuffer::color(mangle_color(img.data[i]));
			return 1;
		}
	}

	lua::fnptr gui_loadbitmappng(lua_func_misc, "gui.bitmap_load_png", [](lua::state& L,
		const std::string& fname) -> int {
		std::string name2;
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			name2 = L.get_string(2, fname.c_str());
		std::string filename = zip::resolverel(name, name2);
		return bitmap_load_png_fn(L, filename);
	});

	lua::fnptr gui_loadbitmappng2(lua_func_misc, "gui.bitmap_load_png_str", [](lua::state& L,
		const std::string& fname) -> int {
		std::string contents = base64_decode(L.get_string(1, fname.c_str()));
		std::istringstream strm(contents);
		return bitmap_load_png_fn(L, strm);
	});

	lua::fnptr gui_savebitmappng(lua_func_misc, "gui.bitmap_save_png", [](lua::state& L,
		const std::string& fname) -> int {
		int slot = 1;
		while(L.type(slot) == LUA_TSTRING)
			slot++;
		if(lua::_class<lua_bitmap>::is(L, slot))
			return lua::_class<lua_bitmap>::get(L, slot, fname.c_str())->_save_png(L, fname, false);
		else if(lua::_class<lua_dbitmap>::is(L, slot))
			return lua::_class<lua_dbitmap>::get(L, slot, fname.c_str())->_save_png(L, fname, false);
		else
			throw std::runtime_error("Expected BITMAP or DBITMAP as first non-string argument for " +
				fname);
		return 0;
	});

	int bitmap_palette_fn(lua::state& L, std::istream& s)
	{
		lua_palette* p = lua::_class<lua_palette>::create(L);
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

	lua::fnptr gui_loadpalette(lua_func_misc, "gui.bitmap_load_pal", [](lua::state& L,
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

	lua::fnptr gui_loadpalette2(lua_func_misc, "gui.bitmap_load_pal_str", [](lua::state& L,
		const std::string& fname) -> int {
		std::string content = L.get_string(1, fname.c_str());
		std::istringstream s(content);
		return bitmap_palette_fn(L, s);
	});

	lua::fnptr gui_dpalette(lua_func_misc, "gui.palette_debug", [](lua::state& L,
		const std::string& fname) -> int {
		lua_palette* p = lua::_class<lua_palette>::get(L, 1, fname.c_str());
		return p->debug(L, fname);
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

	lua::fnptr adjust_trans(lua_func_misc, "gui.adjust_transparency", [](lua::state& L,
		const std::string& fname) -> int {
		if(lua::_class<lua_dbitmap>::is(L, 1))
			return lua::_class<lua_dbitmap>::get(L, 1, fname.c_str())->adjust_transparency(L, fname);
		else if(lua::_class<lua_palette>::is(L, 1))
			return lua::_class<lua_palette>::get(L, 1, fname.c_str())->adjust_transparency(L, fname);
		else
			throw std::runtime_error("Expected BITMAP or PALETTE as argument 1 for "
				"gui.adjust_transparency");
		return 0;
	});

	lua::_class<lua_palette> class_palette("PALETTE");
	lua::_class<lua_bitmap> class_bitmap("BITMAP");
	lua::_class<lua_dbitmap> class_dbitmap("DBITMAP");
}

/** Palette **/
lua_palette::lua_palette(lua::state& L)
{
	lua::objclass<lua_palette>().bind_multi(L, {
		{"set", &lua_palette::set},
		{"hash", &lua_palette::hash},
		{"debug", &lua_palette::debug},
		{"adjust_transparency", &lua_palette::adjust_transparency},
	});
}

lua_palette::~lua_palette()
{
}

std::string lua_palette::print()
{
	size_t s = colors.size();
	return (stringfmt() << s << " " << ((s != 1) ? "colors" : "color")).str();
}

int lua_palette::set(lua::state& L, const std::string& fname)
{
	uint16_t c = L.get_numeric_argument<uint16_t>(2, fname.c_str());
	int64_t nval = L.get_numeric_argument<int64_t>(3, fname.c_str());
	framebuffer::color nc(nval);
	//The mutex lock protects only the internals of colors array.
	if(this->colors.size() <= c) {
		this->palette_mutex.lock();
		this->colors.resize(static_cast<uint32_t>(c) + 1);
		this->palette_mutex.unlock();
	}
	this->colors[c] = nc;
	return 0;
}

int lua_palette::hash(lua::state& L, const std::string& fname)
{
	sha256 h;
	const int buffersize = 256;
	int bufferuse = 0;
	char buf[buffersize];
	unsigned realsize = 0;
	for(unsigned i = 0; i < this->colors.size(); i++)
		if(this->colors[i].origa) realsize = i + 1;
	for(unsigned i = 0; i < realsize; i++) {
		if(bufferuse + 6 > buffersize) {
			h.write(buf, bufferuse);
			bufferuse = 0;
		}
		serialization::u32b(buf + bufferuse + 0, this->colors[i].orig);
		serialization::u16b(buf + bufferuse + 4, this->colors[i].origa);
		bufferuse += 6;
	}
	if(bufferuse > 0) h.write(buf, bufferuse);
	L.pushlstring(h.read());
	return 1;
}

int lua_palette::debug(lua::state& L, const std::string& fname)
{
	size_t i = 0;
	for(auto c : this->colors)
		messages << "Color #" << (i++) << ": " << c.orig << ":" << c.origa << std::endl;
	return 0;
}

int lua_palette::adjust_transparency(lua::state& L, const std::string& fname)
{
	uint16_t tadj = L.get_numeric_argument<uint16_t>(2, fname.c_str());
	for(auto& c : this->colors)
		c = tadjust(c, tadj);
	return 0;
}

/** BITMAP **/
lua_bitmap::lua_bitmap(lua::state& L, uint32_t w, uint32_t h)
{
	lua::objclass<lua_bitmap>().bind_multi(L, {
		{"draw", &lua_bitmap::draw},
		{"pset", &lua_bitmap::pset},
		{"pget", &lua_bitmap::pget},
		{"size", &lua_bitmap::size},
		{"hash", &lua_bitmap::hash},
		{"blit", &lua_bitmap::blit},
		{"save_png", &lua_bitmap::save_png},
	});
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

int lua_bitmap::draw(lua::state& L, const std::string& fname)
{
	return _draw(L, fname, true);
}

int lua_bitmap::_draw(lua::state& L, const std::string& fname, bool is_method)
{
	if(!lua_render_ctx)
		return 0;
	int arg_x = is_method ? 2 : 1;
	int arg_y = arg_x + 1;
	int arg_b = is_method ? 1 : 3;
	int32_t x = L.get_numeric_argument<int32_t>(arg_x, fname.c_str());
	int32_t y = L.get_numeric_argument<int32_t>(arg_y, fname.c_str());
	auto b = lua::_class<lua_bitmap>::pin(L, arg_b, fname.c_str());
	auto p = lua::_class<lua_palette>::pin(L, 4, fname.c_str());
	lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b, p);
	return 0;
}

int lua_bitmap::pset(lua::state& L, const std::string& fname)
{
	uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	uint16_t c = L.get_numeric_argument<uint16_t>(4, fname.c_str());
	if(x >= this->width || y >= this->height)
		return 0;
	this->pixels[y * this->width + x] = c;
	return 0;
}

int lua_bitmap::pget(lua::state& L, const std::string& fname)
{
	uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	if(x >= this->width || y >= this->height)
		return 0;
	L.pushnumber(this->pixels[y * this->width + x]);
	return 1;
}

int lua_bitmap::size(lua::state& L, const std::string& fname)
{
	L.pushnumber(this->width);
	L.pushnumber(this->height);
	return 2;
}

int lua_bitmap::hash(lua::state& L, const std::string& fname)
{
	sha256 h;
	const int buffersize = 256;
	int bufferuse = 0;
	char buf[buffersize];
	memset(buf, 0, buffersize);
	serialization::u64b(buf + 0, this->width);
	serialization::u64b(buf + 8, this->height);
	bufferuse = 16;
	for(unsigned i = 0; i < this->width * this->height; i++) {
		if(bufferuse + 2 > buffersize) {
			h.write(buf, bufferuse);
			bufferuse = 0;
		}
		serialization::u16b(buf + bufferuse + 0, this->pixels[i]);
		bufferuse += 2;
	}
	if(bufferuse > 0) h.write(buf, bufferuse);
	L.pushlstring(h.read());
	return 1;
}

int lua_bitmap::blit(lua::state& L, const std::string& fname)
{
	uint32_t dx = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t dy = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	bool src_d = lua::_class<lua_dbitmap>::is(L, 4);
	bool src_p = lua::_class<lua_bitmap>::is(L, 4);
	if(!src_d && !src_p)
		throw std::runtime_error("Expected BITMAP or DBITMAP as argument 4 for " + fname);
	uint32_t sx = L.get_numeric_argument<uint32_t>(5, fname.c_str());
	uint32_t sy = L.get_numeric_argument<uint32_t>(6, fname.c_str());
	uint32_t w = L.get_numeric_argument<uint32_t>(7, fname.c_str());
	uint32_t h = L.get_numeric_argument<uint32_t>(8, fname.c_str());
	int64_t ck = 0x100000000ULL;
	L.get_numeric_argument<int64_t>(9, ck, fname.c_str());
	if(src_p) {
		lua_bitmap* sb = lua::_class<lua_bitmap>::get(L, 4, fname.c_str());
		if(ck > 65535)
			xblit(srcdest_palette<colorkey_none>(*this, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
		else
			xblit(srcdest_palette<colorkey_palette>(*this, *sb, colorkey_palette(ck)), dx, dy, sx,
				sy, w, h);
	} else
		throw std::runtime_error("If parameter 1 to " + fname + " is paletted, parameter 4 must be "
			"too");
	return 0;
}

int lua_bitmap::save_png(lua::state& L, const std::string& fname)
{
	return _save_png(L, fname, true);
}

int lua_bitmap::_save_png(lua::state& L, const std::string& fname, bool is_method)
{
	int index = is_method ? 2 : 1;
	int oindex = index;
	std::string name, name2;
	if(L.type(index) == LUA_TSTRING) {
		name = L.get_string(index, fname.c_str());
		index++;
	}
	if(L.type(index) == LUA_TSTRING) {
		name2 = L.get_string(index, fname.c_str());
		index++;
	}
	lua_palette* p = lua::_class<lua_palette>::get(L, index + (is_method ? 0 : 1), fname.c_str());
	auto buf = this->save_png(*p);
	if(L.type(oindex) == LUA_TSTRING) {
		std::string filename = zip::resolverel(name, name2);
		std::ofstream strm(filename, std::ios::binary);
		if(!strm)
			throw std::runtime_error("Can't open output file");
		strm.write(&buf[0], buf.size());
		if(!strm)
			throw std::runtime_error("Can't write output file");
		return 0;
	} else {
		std::ostringstream strm;
		strm.write(&buf[0], buf.size());
		L.pushlstring(base64_encode(strm.str()));
		return 1;
	}
}

/** DBITMAP **/
lua_dbitmap::lua_dbitmap(lua::state& L, uint32_t w, uint32_t h)
{
	lua::objclass<lua_dbitmap>().bind_multi(L, {
		{"draw", &lua_dbitmap::draw},
		{"pset", &lua_dbitmap::pset},
		{"pget", &lua_dbitmap::pget},
		{"size", &lua_dbitmap::size},
		{"hash", &lua_dbitmap::hash},
		{"blit", &lua_dbitmap::blit},
		{"save_png", &lua_dbitmap::save_png},
		{"adjust_transparency", &lua_dbitmap::adjust_transparency},
	});
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

int lua_dbitmap::draw(lua::state& L, const std::string& fname)
{
	return _draw(L, fname, true);
}

int lua_dbitmap::_draw(lua::state& L, const std::string& fname, bool is_method)
{
	if(!lua_render_ctx)
		return 0;
	int arg_x = is_method ? 2 : 1;
	int arg_y = arg_x + 1;
	int arg_b = is_method ? 1 : 3;
	int32_t x = L.get_numeric_argument<int32_t>(arg_x, fname.c_str());
	int32_t y = L.get_numeric_argument<int32_t>(arg_y, fname.c_str());
	auto b = lua::_class<lua_dbitmap>::pin(L, arg_b, fname.c_str());
	lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b);
	return 0;
}

int lua_dbitmap::pset(lua::state& L, const std::string& fname)
{
	uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	int64_t c = L.get_numeric_argument<int64_t>(4, fname.c_str());
	if(x >= this->width || y >= this->height)
		return 0;
	this->pixels[y * this->width + x] = framebuffer::color(c);
	return 0;
}

int lua_dbitmap::pget(lua::state& L, const std::string& fname)
{
	uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	if(x >= this->width || y >= this->height)
		return 0;
	L.pushnumber(demultiply_color(this->pixels[y * this->width + x]));
	return 1;
}

int lua_dbitmap::size(lua::state& L, const std::string& fname)
{
	L.pushnumber(this->width);
	L.pushnumber(this->height);
	return 2;
}

int lua_dbitmap::hash(lua::state& L, const std::string& fname)
{
	sha256 h;
	const int buffersize = 256;
	int bufferuse = 0;
	char buf[buffersize];
	memset(buf, 0, buffersize);
	serialization::u64b(buf + 0, this->width);
	serialization::u64b(buf + 4, this->height);
	bufferuse = 16;
	for(unsigned i = 0; i < this->width * this->height; i++) {
		if(bufferuse + 6 > buffersize) {
			h.write(buf, bufferuse);
			bufferuse = 0;
		}
		serialization::u32b(buf + bufferuse + 0, this->pixels[i].orig);
		serialization::u16b(buf + bufferuse + 4, this->pixels[i].origa);
		bufferuse += 6;
	}
	if(bufferuse > 0) h.write(buf, bufferuse);
	L.pushlstring(h.read());
	return 1;
}

int lua_dbitmap::blit(lua::state& L, const std::string& fname)
{
	uint32_t dx = L.get_numeric_argument<uint32_t>(2, fname.c_str());
	uint32_t dy = L.get_numeric_argument<uint32_t>(3, fname.c_str());
	bool src_d = lua::_class<lua_dbitmap>::is(L, 4);
	bool src_p = lua::_class<lua_bitmap>::is(L, 4);
	if(!src_d && !src_p)
		throw std::runtime_error("Expected BITMAP or DBITMAP as argument 4 for gui.bitmap_blit");
	int slot = 5;
	if(src_p)
		slot++;		//Reserve slot 5 for palette.
	uint32_t sx = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
	uint32_t sy = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
	uint32_t w = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
	uint32_t h = L.get_numeric_argument<uint32_t>(slot++, fname.c_str());
	int64_t ck = 0x100000000ULL;
	L.get_numeric_argument<int64_t>(slot++, ck, fname.c_str());

	if(src_d) {
		lua_dbitmap* sb = lua::_class<lua_dbitmap>::get(L, 4, fname.c_str());
		if(ck == 0x100000000ULL)
			xblit(srcdest_direct<colorkey_none>(*this, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
		else
			xblit(srcdest_direct<colorkey_direct>(*this, *sb, colorkey_direct(ck)), dx, dy, sx, sy,
				w, h);
	} else {
		lua_bitmap* sb = lua::_class<lua_bitmap>::get(L, 4, fname.c_str());
		lua_palette* pal = lua::_class<lua_palette>::get(L, 5, fname.c_str());
		if(ck > 65535)
			xblit(srcdest_paletted<colorkey_none>(*this, *sb, *pal, colorkey_none()), dx, dy, sx, sy,
				w, h);
		else
			xblit(srcdest_paletted<colorkey_palette>(*this, *sb, *pal, colorkey_palette(ck)), dx, dy,
				sx, sy, w, h);
	}
	return 0;
}

int lua_dbitmap::save_png(lua::state& L, const std::string& fname)
{
	return _save_png(L, fname, true);
}

int lua_dbitmap::_save_png(lua::state& L, const std::string& fname, bool is_method)
{
	int index = is_method ? 2 : 1;
	std::string name, name2;
	if(L.type(index) == LUA_TSTRING)
		name = L.get_string(index, fname.c_str());
	if(L.type(index + 1) == LUA_TSTRING)
		name2 = L.get_string(index + 1, fname.c_str());
	auto buf = this->save_png();
	if(L.type(index) == LUA_TSTRING) {
		std::string filename = zip::resolverel(name, name2);
		std::ofstream strm(filename, std::ios::binary);
		if(!strm)
			throw std::runtime_error("Can't open output file");
		strm.write(&buf[0], buf.size());
		if(!strm)
			throw std::runtime_error("Can't write output file");
		return 0;
	} else {
		std::ostringstream strm;
		strm.write(&buf[0], buf.size());
		L.pushlstring(base64_encode(strm.str()));
		return 1;
	}
}

int lua_dbitmap::adjust_transparency(lua::state& L, const std::string& fname)
{
	uint16_t tadj = L.get_numeric_argument<uint16_t>(2, fname.c_str());
	for(auto& c : this->pixels)
		c = tadjust(c, tadj);
	return 0;
}
