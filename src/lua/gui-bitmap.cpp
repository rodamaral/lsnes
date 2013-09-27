#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/png-decoder.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include "lua/bitmap.hpp"
#include <vector>
#include <sstream>

lua_bitmap::lua_bitmap(uint32_t w, uint32_t h)
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

lua_dbitmap::lua_dbitmap(uint32_t w, uint32_t h)
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

lua_palette::lua_palette()
{
	palette_mutex = &mutex::aquire();
}

lua_palette::~lua_palette()
{
	delete palette_mutex;
}

std::string lua_palette::print()
{
	size_t s = colors.size();
	return (stringfmt() << s << " " << ((s != 1) ? "colors" : "color")).str();
}

namespace
{
	struct render_object_bitmap : public render_object
	{
		render_object_bitmap(int32_t _x, int32_t _y, lua_obj_pin<lua_bitmap>* _bitmap,
			lua_obj_pin<lua_palette>* _palette) throw()
		{
			x = _x;
			y = _y;
			b = _bitmap;
			b2 = NULL;
			p = _palette;
		}

		render_object_bitmap(int32_t _x, int32_t _y, lua_obj_pin<lua_dbitmap>* _bitmap) throw()
		{
			x = _x;
			y = _y;
			b = NULL;
			b2 = _bitmap;
			p = NULL;
		}

		~render_object_bitmap() throw()
		{
			delete b;
			delete b2;
			delete p;
		}

		bool kill_request(void* obj) throw()
		{
			return kill_request_ifeq(unbox_any_pin(p), obj) ||
				kill_request_ifeq(unbox_any_pin(b), obj) ||
				kill_request_ifeq(unbox_any_pin(b2), obj);
		}

		template<bool T> void composite_op(struct framebuffer<T>& scr) throw()
		{
			if(p)
				p->object()->palette_mutex->lock();
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			size_t pallim = 0;
			size_t w, h;
			premultiplied_color* palette;
			if(b) {
				palette = &p->object()->colors[0];
				for(auto& c : p->object()->colors)
					c.set_palette(scr);
				pallim = p->object()->colors.size();
				w = b->object()->width;
				h = b->object()->height;
			} else {
				for(auto& c : b2->object()->pixels)
					c.set_palette(scr);
				w = b2->object()->width;
				h = b2->object()->height;
			}

			int32_t xmin = 0;
			int32_t xmax = w;
			int32_t ymin = 0;
			int32_t ymax = h;
			clip_range(originx, scr.get_width(), x, xmin, xmax);
			clip_range(originy, scr.get_height(), y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer<T>::element_t* rptr = scr.rowptr(y + r + originy);
				size_t eptr = x + xmin + originx;
				if(b)
					for(int32_t c = xmin; c < xmax; c++, eptr++) {
						uint16_t i = b->object()->pixels[r * b->object()->width + c];
						if(i < pallim)
							palette[i].apply(rptr[eptr]);
					}
				else
					for(int32_t c = xmin; c < xmax; c++, eptr++)
						b2->object()->pixels[r * b2->object()->width + c].apply(rptr[eptr]);
			}
			if(p)
				p->object()->palette_mutex->unlock();
		}
		void operator()(struct framebuffer<false>& x) throw() { composite_op(x); }
		void operator()(struct framebuffer<true>& x) throw() { composite_op(x); }
	private:
		int32_t x;
		int32_t y;
		lua_obj_pin<lua_bitmap>* b;
		lua_obj_pin<lua_dbitmap>* b2;
		lua_obj_pin<lua_palette>* p;
	};

	function_ptr_luafun gui_bitmap("gui.bitmap_draw", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		if(lua_class<lua_bitmap>::is(LS, 3)) {
			lua_class<lua_bitmap>::get(LS, 3, fname.c_str());
			lua_class<lua_palette>::get(LS, 4, fname.c_str());
			auto b = lua_class<lua_bitmap>::pin(LS, 3, fname.c_str());
			auto p = lua_class<lua_palette>::pin(LS, 4, fname.c_str());
			lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b, p);
		} else if(lua_class<lua_dbitmap>::is(LS, 3)) {
			lua_class<lua_dbitmap>::get(LS, 3, fname.c_str());
			auto b = lua_class<lua_dbitmap>::pin(LS, 3, fname.c_str());
			lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b);
		} else
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 3 for gui.bitmap_draw.");
		return 0;
	});

	function_ptr_luafun gui_cpalette("gui.palette_new", [](lua_State* LS, const std::string& fname) -> int {
		lua_class<lua_palette>::create(LS);
		return 1;
	});

	function_ptr_luafun gui_cbitmap("gui.bitmap_new", [](lua_State* LS, const std::string& fname) -> int {
		uint32_t w = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		uint32_t h = get_numeric_argument<uint32_t>(LS, 2, fname.c_str());
		bool d = get_boolean_argument(LS, 3, fname.c_str());
		if(d) {
			int64_t c = -1;
			get_numeric_argument<int64_t>(LS, 4, c, fname.c_str());
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(LS, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = premultiplied_color(c);
		} else {
			uint16_t c = 0;
			get_numeric_argument<uint16_t>(LS, 4, c, fname.c_str());
			lua_bitmap* b = lua_class<lua_bitmap>::create(LS, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = c;
		}
		return 1;
	});

	function_ptr_luafun gui_epalette("gui.palette_set", [](lua_State* LS, const std::string& fname) -> int {
		lua_palette* p = lua_class<lua_palette>::get(LS, 1, fname.c_str());
		uint16_t c = get_numeric_argument<uint16_t>(LS, 2, fname.c_str());
		int64_t nval = get_numeric_argument<int64_t>(LS, 3, fname.c_str());
		premultiplied_color nc(nval);
		//The mutex lock protects only the internals of colors array.
		if(p->colors.size() <= c) {
			p->palette_mutex->lock();
			p->colors.resize(static_cast<uint32_t>(c) + 1);
			p->palette_mutex->unlock();
		}
		p->colors[c] = nc;
		return 0;
	});

	function_ptr_luafun pset_bitmap("gui.bitmap_pset", [](lua_State* LS, const std::string& fname) -> int {
		uint32_t x = get_numeric_argument<uint32_t>(LS, 2, fname.c_str());
		uint32_t y = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		if(lua_class<lua_bitmap>::is(LS, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(LS, 1, fname.c_str());
			uint16_t c = get_numeric_argument<uint16_t>(LS, 4, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			b->pixels[y * b->width + x] = c;
		} else if(lua_class<lua_dbitmap>::is(LS, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(LS, 1, fname.c_str());
			int64_t c = get_numeric_argument<int64_t>(LS, 4, fname.c_str());
			if(x >= b->width || y >= b->height)
				return 0;
			b->pixels[y * b->width + x] = premultiplied_color(c);
		} else {
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_pset.");
		}
		return 0;
	});

	function_ptr_luafun size_bitmap("gui.bitmap_size", [](lua_State* LS, const std::string& fname) -> int {
		if(lua_class<lua_bitmap>::is(LS, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(LS, 1, fname.c_str());
			lua_pushnumber(LS, b->width);
			lua_pushnumber(LS, b->height);
		} else if(lua_class<lua_dbitmap>::is(LS, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(LS, 1, fname.c_str());
			lua_pushnumber(LS, b->width);
			lua_pushnumber(LS, b->height);
		} else {
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_size.");
		}
		return 2;
	});

	struct colorkey_none
	{
		bool iskey(uint16_t& c) const { return false; }
		bool iskey(premultiplied_color& c) const { return false; }
	};

	struct colorkey_direct
	{
		colorkey_direct(uint64_t _ck)
		{
			premultiplied_color c(_ck);
			ck = c.orig;
			cka = c.origa;
		}
		bool iskey(premultiplied_color& c) const { return (c.orig == ck && c.origa == cka); }
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
			premultiplied_color c = sarray[sidx];
			if(!ckey.iskey(c))
				darray[didx] = c;
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		premultiplied_color* sarray;
		premultiplied_color* darray;
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
		typedef premultiplied_color ptype;
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
		premultiplied_color* darray;
		premultiplied_color* pal;
		uint32_t limit;
		premultiplied_color transparent;
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

	function_ptr_luafun blit_bitmap("gui.bitmap_blit", [](lua_State* LS, const std::string& fname) -> int {
		int slot = 1;
		int dsts = 0;
		int srcs = 0;
		bool dst_d = lua_class<lua_dbitmap>::is(LS, dsts = slot);
		bool dst_p = lua_class<lua_bitmap>::is(LS, slot++);
		if(!dst_d && !dst_p)
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_blit");
		uint32_t dx = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		uint32_t dy = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		bool src_d = lua_class<lua_dbitmap>::is(LS, srcs = slot);
		bool src_p = lua_class<lua_bitmap>::is(LS, slot++);
		if(!src_d && !src_p)
			throw std::runtime_error("Expected BITMAP or DBITMAP as argument 4 for gui.bitmap_blit");
		if(dst_d && src_p)
			slot++;		//Reserve slot 5 for palette.
		uint32_t sx = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		uint32_t sy = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		uint32_t w = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		uint32_t h = get_numeric_argument<uint32_t>(LS, slot++, fname.c_str());
		int64_t ck = 0x100000000ULL;
		get_numeric_argument<int64_t>(LS, slot++, ck, fname.c_str());

		if(dst_d && src_d) {
			lua_dbitmap* db = lua_class<lua_dbitmap>::get(LS, dsts, fname.c_str());
			lua_dbitmap* sb = lua_class<lua_dbitmap>::get(LS, srcs, fname.c_str());
			if(ck == 0x100000000ULL)
				blit(srcdest_direct<colorkey_none>(*db, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
			else
				blit(srcdest_direct<colorkey_direct>(*db, *sb, colorkey_direct(ck)), dx, dy, sx, sy, w,
					h);
		} else if(dst_p && src_p) {
			lua_bitmap* db = lua_class<lua_bitmap>::get(LS, dsts, fname.c_str());
			lua_bitmap* sb = lua_class<lua_bitmap>::get(LS, srcs, fname.c_str());
			if(ck > 65535)
				blit(srcdest_palette<colorkey_none>(*db, *sb, colorkey_none()), dx, dy, sx, sy, w, h);
			else
				blit(srcdest_palette<colorkey_palette>(*db, *sb, colorkey_palette(ck)), dx, dy, sx, sy,
					w, h);
		} else if(dst_d && src_p) {
			lua_dbitmap* db = lua_class<lua_dbitmap>::get(LS, dsts, fname.c_str());
			lua_bitmap* sb = lua_class<lua_bitmap>::get(LS, srcs, fname.c_str());
			lua_palette* pal = lua_class<lua_palette>::get(LS, srcs + 1, fname.c_str());
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

	int bitmap_load_fn(lua_State* LS, std::function<lua_loaded_bitmap()> src)
	{
		uint32_t w, h;
		auto bitmap = src();
		if(bitmap.d) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(LS, bitmap.w, bitmap.h);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = premultiplied_color(bitmap.bitmap[i]);
			return 1;
		} else {
			lua_bitmap* b = lua_class<lua_bitmap>::create(LS, bitmap.w, bitmap.h);
			lua_palette* p = lua_class<lua_palette>::create(LS);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = bitmap.bitmap[i];
			p->colors.resize(bitmap.palette.size());
			for(size_t i = 0; i < bitmap.palette.size(); i++)
				p->colors[i] = premultiplied_color(bitmap.palette[i]);
			return 2;
		}
	}

	function_ptr_luafun gui_loadbitmap("gui.bitmap_load", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		return bitmap_load_fn(LS, [&name]() -> lua_loaded_bitmap { return lua_loaded_bitmap::load(name); });
	});

	function_ptr_luafun gui_loadbitmap2("gui.bitmap_load_str", [](lua_State* LS, const std::string& fname) -> int {
		std::string contents = get_string_argument(LS, 1, fname.c_str());
		return bitmap_load_fn(LS, [&contents]() -> lua_loaded_bitmap {
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

	int bitmap_load_png_fn(lua_State* LS, std::function<void(png_decoded_image&)> src)
	{
		png_decoded_image img;
		src(img);
		if(img.has_palette) {
			lua_bitmap* b = lua_class<lua_bitmap>::create(LS, img.width, img.height);
			lua_palette* p = lua_class<lua_palette>::create(LS);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = img.data[i];
			p->colors.resize(img.palette.size());
			for(size_t i = 0; i < img.palette.size(); i++)
				p->colors[i] = premultiplied_color(mangle_color(img.palette[i]));
			return 2;
		} else {
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(LS, img.width, img.height);
			for(size_t i = 0; i < img.width * img.height; i++)
				b->pixels[i] = premultiplied_color(mangle_color(img.data[i]));
			return 1;
		}
	}

	function_ptr_luafun gui_loadbitmappng("gui.bitmap_load_png", [](lua_State* LS, const std::string& fname)
		-> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		return bitmap_load_png_fn(LS, [&name](png_decoded_image& img) { decode_png(name, img); });
	});

	function_ptr_luafun gui_loadbitmappng2("gui.bitmap_load_png_str", [](lua_State* LS, const std::string& fname)
		-> int {
		std::string contents = base64_decode(get_string_argument(LS, 1, fname.c_str()));
		return bitmap_load_png_fn(LS, [&contents](png_decoded_image& img) {
			std::istringstream strm(contents);
			decode_png(strm, img);
		});
	});

	int bitmap_palette_fn(lua_State* LS, std::istream& s)
	{
		lua_palette* p = lua_class<lua_palette>::create(LS);
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
					p->colors.push_back(premultiplied_color(-1));
				else
					p->colors.push_back(premultiplied_color((ca << 24) | (cr << 16)
						| (cg << 8) | cb));
			} else if(r = regex("[ \t]*([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]*", line)) {
				int64_t cr, cg, cb;
				cr = parse_value<uint8_t>(r[1]);
				cg = parse_value<uint8_t>(r[2]);
				cb = parse_value<uint8_t>(r[3]);
				p->colors.push_back(premultiplied_color((cr << 16) | (cg << 8) | cb));
			} else if(regex_match("[ \t]*transparent[ \t]*", line)) {
				p->colors.push_back(premultiplied_color(-1));
			} else if(!regex_match("[ \t]*(#.*)?", line))
				throw std::runtime_error("Invalid line format (" + line + ")");
		}
		return 1;
	}
	
	function_ptr_luafun gui_loadpalette("gui.bitmap_load_pal", [](lua_State* LS, const std::string& fname)
		-> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::istream& s = open_file_relative(name, "");
		try {
			int r = bitmap_palette_fn(LS, s);
			delete &s;
			return r;
		} catch(...) {
			delete &s;
			throw;
		}
	});

	function_ptr_luafun gui_loadpalette2("gui.bitmap_load_pal_str", [](lua_State* LS, const std::string& fname)
		-> int {
		std::string content = get_string_argument(LS, 1, fname.c_str());
		std::istringstream s(content);
		return bitmap_palette_fn(LS, s);
	});

	function_ptr_luafun gui_dpalette("gui.palette_debug", [](lua_State* LS, const std::string& fname) -> int {
		lua_palette* p = lua_class<lua_palette>::get(LS, 1, fname.c_str());
		size_t i = 0;
		for(auto c : p->colors)
			messages << "Color #" << (i++) << ": " << c.orig << ":" << c.origa << std::endl;
		return 0;
	});
}

DECLARE_LUACLASS(lua_palette, "PALETTE");
DECLARE_LUACLASS(lua_bitmap, "BITMAP");
DECLARE_LUACLASS(lua_dbitmap, "DBITMAP");
