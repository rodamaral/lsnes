#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/minmax.hpp"
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

	struct operand_dbitmap
	{
		typedef framebuffer::color pixel_t;
		typedef framebuffer::color rpixel_t;
		operand_dbitmap(lua_dbitmap& _bitmap)
			: bitmap(_bitmap), _transparent(-1)
		{
			pixels = &bitmap.pixels[0];
		}
		size_t get_width() { return bitmap.width; }
		size_t get_height() { return bitmap.height; }
		const rpixel_t& read(size_t idx) { return pixels[idx]; }
		const pixel_t& lookup(const rpixel_t& p) { return p; }
		void write(size_t idx, const pixel_t& v) { pixels[idx] = v; }
		bool is_opaque(const rpixel_t& p) { return p.origa > 0; }
		const pixel_t& transparent() { return _transparent; }
	private:
		lua_dbitmap& bitmap;
		pixel_t* pixels;
		framebuffer::color _transparent;
	};

	struct operand_bitmap
	{
		typedef uint16_t pixel_t;
		typedef uint16_t rpixel_t;
		operand_bitmap(lua_bitmap& _bitmap)
			: bitmap(_bitmap)
		{
			pixels = &bitmap.pixels[0];
		}
		size_t get_width() { return bitmap.width; }
		size_t get_height() { return bitmap.height; }
		const rpixel_t& read(size_t idx) { return pixels[idx]; }
		const pixel_t& lookup(const rpixel_t& p) { return p; }
		void write(size_t idx, const pixel_t& v) { pixels[idx] = v; }
		bool is_opaque(const rpixel_t& p) { return p > 0; }
		pixel_t transparent() { return 0; }
	private:
		lua_bitmap& bitmap;
		pixel_t* pixels;
	};

	struct operand_bitmap_pal
	{
		typedef framebuffer::color pixel_t;
		typedef uint16_t rpixel_t;
		operand_bitmap_pal(lua_bitmap& _bitmap, lua_palette& _palette)
			: bitmap(_bitmap), palette(_palette), _transparent(-1)
		{
			pixels = &bitmap.pixels[0];
			limit = palette.colors.size();
			pal = &palette.colors[0];
		}
		size_t get_width() { return bitmap.width; }
		size_t get_height() { return bitmap.height; }
		const rpixel_t& read(size_t idx) { return pixels[idx]; }
		const pixel_t& lookup(const rpixel_t& p) { return *((p < limit) ? pal + p : &_transparent); }
		bool is_opaque(const rpixel_t& p) { return p > 0; }
		const pixel_t& transparent() { return _transparent; }
	private:
		lua_bitmap& bitmap;
		lua_palette& palette;
		uint16_t* pixels;
		framebuffer::color* pal;
		uint32_t limit;
		framebuffer::color _transparent;
	};

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

	template<class _src, class _dest, class colorkey> struct srcdest
	{
		srcdest(_dest Xdest, _src Xsrc, const colorkey& _ckey)
			: dest(Xdest), src(Xsrc), ckey(_ckey)
		{
			swidth = src.get_width();
			sheight = src.get_height();
			dwidth = dest.get_width();
			dheight = dest.get_height();
		}
		void copy(size_t didx, size_t sidx)
		{
			typename _src::rpixel_t c = src.read(sidx);
			if(!ckey.iskey(c))
				dest.write(didx, src.lookup(c));
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		_dest dest;
		_src src;
		colorkey ckey;
	};

	template<class _src, class _dest, class colorkey> srcdest<_src, _dest, colorkey> mk_srcdest(_dest dest,
		_src src, const colorkey& ckey)
	{
		return srcdest<_src, _dest, colorkey>(dest, src, ckey);
	}

	struct srcdest_priority
	{
		srcdest_priority(lua_bitmap& dest, lua_bitmap& src)
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
			if(darray[didx] < c)
				darray[didx] = c;
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		uint16_t* sarray;
		uint16_t* darray;
	};

	enum porterduff_oper
	{
		PD_SRC,
		PD_ATOP,
		PD_OVER,
		PD_IN,
		PD_OUT,
		PD_DEST,
		PD_DEST_ATOP,
		PD_DEST_OVER,
		PD_DEST_IN,
		PD_DEST_OUT,
		PD_CLEAR,
		PD_XOR
	};

	porterduff_oper get_pd_oper(const std::string& oper)
	{
		if(oper == "Src") return PD_SRC;
		if(oper == "Atop") return PD_ATOP;
		if(oper == "Over") return PD_OVER;
		if(oper == "In") return PD_IN;
		if(oper == "Out") return PD_OUT;
		if(oper == "Dest") return PD_DEST;
		if(oper == "DestAtop") return PD_DEST_ATOP;
		if(oper == "DestOver") return PD_DEST_OVER;
		if(oper == "DestIn") return PD_DEST_IN;
		if(oper == "DestOut") return PD_DEST_OUT;
		if(oper == "Clear") return PD_CLEAR;
		if(oper == "Xor") return PD_XOR;
		(stringfmt() << "Bad Porter-Duff operator '" << oper << "'").throwex();
	}

	template<porterduff_oper oper, class _src, class _dest> struct srcdest_porterduff
	{
		srcdest_porterduff(_dest Xdest, _src Xsrc)
			: dest(Xdest), src(Xsrc)
		{
			swidth = src.get_width();
			sheight = src.get_height();
			dwidth = dest.get_width();
			dheight = dest.get_height();
		}
		void copy(size_t didx, size_t sidx)
		{
			typename _dest::rpixel_t vd = dest.read(didx);
			typename _src::rpixel_t vs = src.read(sidx);
			bool od = dest.is_opaque(vd);
			bool os = src.is_opaque(vs);
			typename _dest::pixel_t ld = dest.lookup(vd);
			typename _src::pixel_t ls = src.lookup(vs);
			typename _dest::pixel_t t = dest.transparent();
			typename _dest::pixel_t r;
			switch(oper) {
			case PD_SRC:		r = ls;				break;
			case PD_ATOP:		r = od ? (os ? ls : ld) : t;	break;
			case PD_OVER:		r = os ? ls : ld;		break;
			case PD_IN:		r = (od & os) ? ls : t;		break;
			case PD_OUT:		r = (!od && os) ? ls : t;	break;
			case PD_DEST:		r = ld;				break;
			case PD_DEST_ATOP:	r = os ? (od ? ld : ls) : t;	break;
			case PD_DEST_OVER:	r = od ? ld : ls;		break;
			case PD_DEST_IN:	r = (od & os) ? ld : t;		break;
			case PD_DEST_OUT:	r = (od & !os) ? ld : t;	break;
			case PD_CLEAR:		r = t;				break;
			case PD_XOR:		r = od ? (os ? t : ld) : ls;	break;
			}
			dest.write(didx, r);
		}
		size_t swidth, sheight, dwidth, dheight;
	private:
		_dest dest;
		_src src;
	};

	template<porterduff_oper oper, class _src, class _dest> srcdest_porterduff<oper, _src, _dest>
		mk_porterduff(_dest dest, _src src)
	{
		return srcdest_porterduff<oper, _src, _dest>(dest, src);
	}

	template<class srcdest>
	void xblit_copy(srcdest sd, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h)
	{
		while((dx + w > sd.dwidth || sx + w > sd.swidth) && w > 0)
			w--;
		while((dy + h > sd.dheight || sy + h > sd.sheight) && h > 0)
			h--;
		if(dx + w < w || dy + h < h) return;  //Don't do overflowing blits.
		if(sx + w < w || sy + h < h) return;  //Don't do overflowing blits.
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

	template<class srcdest>
	void xblit_scaled(srcdest sd, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h,
		uint32_t hscl, uint32_t vscl)
	{
		if(!hscl || !vscl) return;
		w = max(static_cast<uint32_t>(sd.dwidth / hscl), w);
		h = max(static_cast<uint32_t>(sd.dheight / vscl), h);
		while((dx + hscl * w > sd.dwidth || sx + w > sd.swidth) && w > 0)
			w--;
		while((dy + vscl * h > sd.dheight || sy + h > sd.sheight) && h > 0)
			h--;
		if(dx + hscl * w < dx || dy + vscl * h < dy) return;  //Don't do overflowing blits.
		if(sx + w < w || sy + h < h) return;  //Don't do overflowing blits.
		size_t sidx = sy * sd.swidth + sx;
		size_t didx = dy * sd.dwidth + dx;
		size_t drskip = sd.dwidth - hscl * w;
		uint32_t _w = hscl * w;
		for(uint32_t j = 0; j < vscl * h; j++) {
			uint32_t _sidx = sidx;
			for(uint32_t i = 0; i < _w ; i += hscl) {
				for(uint32_t k = 0; k < hscl; k++)
					sd.copy(didx + k, _sidx);
				_sidx++;
				didx+=hscl;
			}
			if((j % vscl) == vscl - 1)
				sidx += sd.swidth;
			didx += drskip;
		}
	}

	template<bool scaled, class srcdest>
	inline void xblit(srcdest sd, uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy, uint32_t w, uint32_t h,
		uint32_t hscl, uint32_t vscl)
	{
		if(scaled)
			xblit_scaled(sd, dx, dy, sx, sy, w, h, hscl, vscl);
		else
			xblit_copy(sd, dx, dy, sx, sy, w, h);
	}

	template<bool scaled, class src, class dest>
	inline void xblit_pal(dest _dest, src _src, uint64_t ck, uint32_t dx, uint32_t dy, uint32_t sx,
		uint32_t sy, uint32_t w, uint32_t h, uint32_t hscl, uint32_t vscl)
	{
		if(ck > 65535)
			xblit<scaled>(mk_srcdest(_dest, _src, colorkey_none()), dx, dy, sx, sy, w, h,
				hscl, vscl);
		else
			xblit<scaled>(mk_srcdest(_dest, _src, colorkey_palette(ck)), dx, dy, sx, sy, w, h,
				hscl, vscl);
	}

	template<bool scaled, class src, class dest>
	inline void xblit_dir(dest _dest, src _src, uint64_t ck, uint32_t dx, uint32_t dy, uint32_t sx,
		uint32_t sy, uint32_t w, uint32_t h, uint32_t hscl, uint32_t vscl)
	{
		if(ck == 0x100000000ULL)
			xblit<scaled>(mk_srcdest(_dest, _src, colorkey_none()), dx, dy, sx, sy, w, h,
				hscl, vscl);
		else
			xblit<scaled>(mk_srcdest(_dest, _src, colorkey_direct(ck)), dx, dy, sx, sy, w, h,
				hscl, vscl);
	}

	template<bool scaled, porterduff_oper oper, class src, class dest>
	inline void xblit_pduff2(dest _dest, src _src, uint32_t dx, uint32_t dy, uint32_t sx,
		uint32_t sy, uint32_t w, uint32_t h, uint32_t hscl, uint32_t vscl)
	{
		xblit<scaled>(mk_porterduff<oper>(_dest, _src), dx, dy, sx, sy, w, h, hscl, vscl);
	}

	template<bool scaled, class src, class dest>
	inline void xblit_pduff(dest _dest, src _src, uint32_t dx, uint32_t dy, uint32_t sx,
		uint32_t sy, uint32_t w, uint32_t h, uint32_t hscl, uint32_t vscl, porterduff_oper oper)
	{
		switch(oper) {
		case PD_ATOP:
			xblit_pduff2<scaled, PD_ATOP>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_CLEAR:
			xblit_pduff2<scaled, PD_CLEAR>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_DEST:
			xblit_pduff2<scaled, PD_DEST>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_DEST_ATOP:
			xblit_pduff2<scaled, PD_DEST_ATOP>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_DEST_IN:
			xblit_pduff2<scaled, PD_DEST_IN>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_DEST_OUT:
			xblit_pduff2<scaled, PD_DEST_OUT>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_DEST_OVER:
			xblit_pduff2<scaled, PD_DEST_OVER>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_IN:
			xblit_pduff2<scaled, PD_IN>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_OUT:
			xblit_pduff2<scaled, PD_OUT>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_OVER:
			xblit_pduff2<scaled, PD_OVER>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_SRC:
			xblit_pduff2<scaled, PD_SRC>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		case PD_XOR:
			xblit_pduff2<scaled, PD_XOR>(_dest, _src, dx, dy, sx, sy, w, h, hscl, vscl);
			break;
		}
	}

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

	int bitmap_palette_fn(lua::state& L, std::istream& s)
	{
		lua_palette* p = lua::_class<lua_palette>::create(L);
		while(s) {
			std::string line;
			std::getline(s, line);
			istrip_CR(line);
			regex_results r;
			if(!regex_match("[ \t]*(#.*)?", line)) {
				//Nothing.
			} else if(r = regex("[ \t]*([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]*", line)) {
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
			} else if(r = regex("[ \t]*([^ \t]|[^ \t].*[^ \t])[ \t]*", line)) {
				p->colors.push_back(framebuffer::color(r[1]));
			} else
				throw std::runtime_error("Invalid line format (" + line + ")");
		}
		return 1;
	}

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

	lua::_class<lua_loaded_bitmap> class_loaded_bitmap(lua_class_gui, "IMAGELOADER", {
		{"load", lua_loaded_bitmap::load<false>},
		{"load_str", lua_loaded_bitmap::load_str<false>},
		{"load_png", lua_loaded_bitmap::load<true>},
		{"load_png_str", lua_loaded_bitmap::load_str<true>},
	});

	lua::_class<lua_palette> class_palette(lua_class_gui, "PALETTE", {
		{"new", lua_palette::create},
		{"load", lua_palette::load},
		{"load_str", lua_palette::load_str},
	}, {
		{"set", &lua_palette::set},
		{"hash", &lua_palette::hash},
		{"debug", &lua_palette::debug},
		{"adjust_transparency", &lua_palette::adjust_transparency},
	}, &lua_palette::print);

	lua::_class<lua_bitmap> class_bitmap(lua_class_gui, "BITMAP", {
		{"new", lua_bitmap::create},
	}, {
		{"draw", &lua_bitmap::draw},
		{"pset", &lua_bitmap::pset},
		{"pget", &lua_bitmap::pget},
		{"size", &lua_bitmap::size},
		{"hash", &lua_bitmap::hash},
		{"blit", &lua_bitmap::blit<false, false>},
		{"blit_priority", &lua_bitmap::blit_priority<false>},
		{"blit_scaled", &lua_bitmap::blit<true, false>},
		{"blit_scaled_priority", &lua_bitmap::blit_priority<true>},
		{"blit_porterduff", &lua_bitmap::blit<false, true>},
		{"blit_scaled_porterduff", &lua_bitmap::blit<true, true>},
		{"save_png", &lua_bitmap::save_png},
	}, &lua_bitmap::print);

	lua::_class<lua_dbitmap> class_dbitmap(lua_class_gui, "DBITMAP", {
		{"new", lua_dbitmap::create},
	}, {
		{"draw", &lua_dbitmap::draw},
		{"pset", &lua_dbitmap::pset},
		{"pget", &lua_dbitmap::pget},
		{"size", &lua_dbitmap::size},
		{"hash", &lua_dbitmap::hash},
		{"blit", &lua_dbitmap::blit<false, false>},
		{"blit_scaled", &lua_dbitmap::blit<true, false>},
		{"blit_porterduff", &lua_dbitmap::blit<false, true>},
		{"blit_scaled_porterduff", &lua_dbitmap::blit<true, true>},
		{"save_png", &lua_dbitmap::save_png},
		{"adjust_transparency", &lua_dbitmap::adjust_transparency},
	}, &lua_dbitmap::print);
}

/** Palette **/
lua_palette::lua_palette(lua::state& L)
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

int lua_palette::create(lua::state& L, lua::parameters& P)
{
	lua::_class<lua_palette>::create(L);
	return 1;
}

int lua_palette::load(lua::state& L, lua::parameters& P)
{
	std::string name, name2;

	P(name, P.optional(name2, ""));

	std::istream& s = zip::openrel(name, name2);
	try {
		int r = bitmap_palette_fn(L, s);
		delete &s;
		return r;
	} catch(...) {
		delete &s;
		throw;
	}
}

int lua_palette::load_str(lua::state& L, lua::parameters& P)
{
	std::string content;

	P(content);

	std::istringstream s(content);
	return bitmap_palette_fn(L, s);
}

int lua_palette::set(lua::state& L, lua::parameters& P)
{
	framebuffer::color nc;
	uint16_t c;

	P(P.skipped(), c, nc);

	//The mutex lock protects only the internals of colors array.
	if(this->colors.size() <= c) {
		this->palette_mutex.lock();
		this->colors.resize(static_cast<uint32_t>(c) + 1);
		this->palette_mutex.unlock();
	}
	this->colors[c] = nc;
	return 0;
}

int lua_palette::hash(lua::state& L, lua::parameters& P)
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

int lua_palette::debug(lua::state& L, lua::parameters& P)
{
	size_t i = 0;
	for(auto c : this->colors)
		messages << "Color #" << (i++) << ": " << c.orig << ":" << c.origa << std::endl;
	return 0;
}

int lua_palette::adjust_transparency(lua::state& L, lua::parameters& P)
{
	uint16_t tadj;

	P(P.skipped(), tadj);

	for(auto& c : this->colors)
		c = tadjust(c, tadj);
	return 0;
}

/** BITMAP **/
lua_bitmap::lua_bitmap(lua::state& L, uint32_t w, uint32_t h)
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

int lua_bitmap::create(lua::state& L, lua::parameters& P)
{
	uint32_t w, h;
	uint16_t c;

	P(w, h, P.optional(c, 0));

	lua_bitmap* b = lua::_class<lua_bitmap>::create(L, w, h);
	for(size_t i = 0; i < b->width * b->height; i++)
		b->pixels[i] = c;
	return 1;
}

int lua_bitmap::draw(lua::state& L, lua::parameters& P)
{
	int32_t x, y;
	lua::objpin<lua_bitmap> b;
	lua::objpin<lua_palette> p;

	if(!lua_render_ctx) return 0;

	P(b, x, y, p);

	lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b, p);
	return 0;
}

int lua_bitmap::pset(lua::state& L, lua::parameters& P)
{
	uint32_t x, y;
	uint16_t c;

	P(P.skipped(), x, y, c);

	if(x >= this->width || y >= this->height)
		return 0;
	this->pixels[y * this->width + x] = c;
	return 0;
}

int lua_bitmap::pget(lua::state& L, lua::parameters& P)
{
	uint32_t x, y;

	P(P.skipped(), x, y);

	if(x >= this->width || y >= this->height)
		return 0;
	L.pushnumber(this->pixels[y * this->width + x]);
	return 1;
}

int lua_bitmap::size(lua::state& L, lua::parameters& P)
{
	L.pushnumber(this->width);
	L.pushnumber(this->height);
	return 2;
}

int lua_bitmap::hash(lua::state& L, lua::parameters& P)
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

template<bool scaled, bool porterduff> int lua_bitmap::blit(lua::state& L, lua::parameters& P)
{
	uint32_t dx, dy, sx, sy, w, h, hscl, vscl;
	lua_bitmap* src_p;

	P(P.skipped(), dx, dy, src_p, sx, sy, w, h);
	if(scaled)
		P(hscl, P.optional2(vscl, hscl));

	if(porterduff) {
		porterduff_oper pd_oper = get_pd_oper(P.arg<std::string>());
		xblit_pduff<scaled>(operand_bitmap(*this), operand_bitmap(*src_p), dx, dy, sx, sy, w, h, hscl, vscl,
			pd_oper);
	} else {
		int64_t ck = P.arg_opt<uint64_t>(65536);
		xblit_pal<scaled>(operand_bitmap(*this), operand_bitmap(*src_p), ck, dx, dy, sx, sy, w, h,
			hscl, vscl);
	}
	return 0;
}

template<bool scaled> int lua_bitmap::blit_priority(lua::state& L, lua::parameters& P)
{
	uint32_t dx, dy, sx, sy, w, h, hscl, vscl;
	lua_bitmap* src_p;

	P(P.skipped(), dx, dy, src_p, sx, sy, w, h);
	if(scaled)
		P(hscl, P.optional2(vscl, hscl));

	xblit<scaled>(srcdest_priority(*this, *src_p), dx, dy, sx, sy, w, h, hscl, vscl);
	return 0;
}

int lua_bitmap::save_png(lua::state& L, lua::parameters& P)
{
	std::string name, name2;
	lua_palette* p;
	bool was_filename;

	P(P.skipped());
	if(was_filename = P.is_string()) P(name);
	if(P.is_string()) P(name2);
	P(p);

	auto buf = this->save_png(*p);
	if(was_filename) {
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

int lua_dbitmap::create(lua::state& L, lua::parameters& P)
{
	uint32_t w, h;
	framebuffer::color c;

	P(w, h, P.optional(c, -1));

	lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, w, h);
	for(size_t i = 0; i < b->width * b->height; i++)
		b->pixels[i] = c;
	return 1;
}

int lua_dbitmap::draw(lua::state& L, lua::parameters& P)
{
	int32_t x, y;
	lua::objpin<lua_dbitmap> b;

	if(!lua_render_ctx) return 0;

	P(b, x, y);

	lua_render_ctx->queue->create_add<render_object_bitmap>(x, y, b);
	return 0;
}

int lua_dbitmap::pset(lua::state& L, lua::parameters& P)
{
	uint32_t x, y;
	framebuffer::color c;

	P(P.skipped(), x, y, c);

	if(x >= this->width || y >= this->height)
		return 0;
	this->pixels[y * this->width + x] = c;
	return 0;
}

int lua_dbitmap::pget(lua::state& L, lua::parameters& P)
{
	uint32_t x, y;

	P(P.skipped(), x, y);

	if(x >= this->width || y >= this->height)
		return 0;
	L.pushnumber((this->pixels[y * this->width + x]).asnumber());
	return 1;
}

int lua_dbitmap::size(lua::state& L, lua::parameters& P)
{
	L.pushnumber(this->width);
	L.pushnumber(this->height);
	return 2;
}

int lua_dbitmap::hash(lua::state& L, lua::parameters& P)
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

template<bool scaled, bool porterduff> int lua_dbitmap::blit(lua::state& L, lua::parameters& P)
{
	uint32_t dx, dy, sx, sy, w, h, hscl, vscl;

	P(P.skipped(), dx, dy);

	//DBitmap or Bitmap+Palette.
	bool src_d = P.is<lua_dbitmap>();
	bool src_p = P.is<lua_bitmap>();
	int sidx = P.skip();
	if(!src_d && !src_p)
		P.expected("BITMAP or DBITMAP", sidx);
	int spal;
	if(src_p)
		spal = P.skip();	//Reserve for palette.

	P(sx, sy, w, h);

	if(scaled)
		P(hscl, P.optional2(vscl, hscl));

	int64_t ckx = 0x100000000ULL;
	porterduff_oper pd_oper;
	if(porterduff) {
		pd_oper = get_pd_oper(P.arg<std::string>());
	} else {
		//Hack: Direct-color bitmaps should take color spec, with special NONE value.
		if(src_p)
			ckx = P.arg_opt<int64_t>(0x10000);
		else if(P.is_novalue())
			; //Do nothing.
		else
			ckx = P.arg<framebuffer::color>().asnumber();
	}

	operand_dbitmap dest(*this);
	if(src_d) {
		operand_dbitmap src(*P.arg<lua_dbitmap*>(sidx));
		if(porterduff)
			xblit_pduff<scaled>(dest, src, dx, dy, sx, sy, w, h, hscl, vscl, pd_oper);
		else
			xblit_dir<scaled>(dest, src, ckx, dx, dy, sx, sy, w, h, hscl, vscl);
	} else {
		operand_bitmap_pal src(*P.arg<lua_bitmap*>(sidx), *P.arg<lua_palette*>(spal));
		if(porterduff)
			xblit_pduff<scaled>(dest, src, dx, dy, sx, sy, w, h, hscl, vscl, pd_oper);
		else
			xblit_pal<scaled>(dest, src, ckx, dx, dy, sx, sy, w, h, hscl, vscl);
	}
	return 0;
}

int lua_dbitmap::save_png(lua::state& L, lua::parameters& P)
{
	std::string name, name2;
	lua_palette* p;
	bool was_filename;

	P(P.skipped());
	if(was_filename = P.is_string()) P(name);
	if(P.is_string()) P(name2);

	auto buf = this->save_png();
	if(was_filename) {
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

int lua_dbitmap::adjust_transparency(lua::state& L, lua::parameters& P)
{
	uint16_t tadj;

	P(P.skipped(), tadj);

	for(auto& c : this->pixels)
		c = tadjust(c, tadj);
	return 0;
}


template<bool png> int lua_loaded_bitmap::load(lua::state& L, lua::parameters& P)
{
	std::string name, name2;

	P(name, P.optional(name2, ""));

	if(png) {
		std::string filename = zip::resolverel(name, name2);
		return bitmap_load_png_fn(L, filename);
	} else
		return bitmap_load_fn(L, [&name, &name2]() -> lua_loaded_bitmap {
			std::string name3 = zip::resolverel(name, name2);
			return lua_loaded_bitmap::load(name3);
		});
}

template<bool png> int lua_loaded_bitmap::load_str(lua::state& L, lua::parameters& P)
{
	std::string contents;

	P(contents);

	if(png) {
		contents = base64_decode(contents);
		std::istringstream strm(contents);
		return bitmap_load_png_fn(L, strm);
	} else
		return bitmap_load_fn(L, [&contents]() -> lua_loaded_bitmap {
			std::istringstream strm(contents);
			return lua_loaded_bitmap::load(strm);
		});
}
