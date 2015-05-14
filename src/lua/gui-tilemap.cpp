#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "library/png.hpp"
#include "library/range.hpp"
#include "library/string.hpp"
#include "library/threads.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/zip.hpp"
#include "lua/bitmap.hpp"
#include <vector>
#include <sstream>

namespace
{
	struct tilemap_entry
	{
		tilemap_entry()
		{
		}
		void erase()
		{
			b.clear();
			d.clear();
			p.clear();
		}
		lua::objpin<lua_bitmap> b;
		lua::objpin<lua_dbitmap> d;
		lua::objpin<lua_palette> p;
	};

	struct tilemap
	{
		tilemap(lua::state& L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight);
		static size_t overcommit(size_t _width, size_t _height, size_t _cwidth, size_t _cheight) {
			return lua::overcommit_std_align + 2 * sizeof(tilemap_entry) * (size_t)_width * _height;
		}
		~tilemap()
		{
			threads::alock h(lock);
			CORE().fbuf->render_kill_request(this);
		}
		static int create(lua::state& L, lua::parameters& P);
		template<bool outside> int draw(lua::state& L, lua::parameters& P);
		int get(lua::state& L, lua::parameters& P)
		{
			uint32_t x, y;

			P(P.skipped(), x, y);

			threads::alock h(lock);
			if(x >= width || y >= height)
				return 0;
			tilemap_entry& e = map[y * width + x];
			if(e.b) {
				e.b.luapush(L);
				e.p.luapush(L);
				return 2;
			} else if(e.d) {
				e.d.luapush(L);
				return 1;
			} else
				return 0;
		}
		int set(lua::state& L, lua::parameters& P)
		{
			uint32_t x, y;

			P(P.skipped(), x, y);
			int oidx = P.skip();

			threads::alock h(lock);
			if(x >= width || y >= height)
				return 0;
			tilemap_entry& e = map[y * width + x];
			if(P.is<lua_dbitmap>(oidx)) {
				auto d = P.arg<lua::objpin<lua_dbitmap>>(oidx);
				e.erase();
				e.d = d;
			} else if(P.is<lua_bitmap>(oidx)) {
				auto b = P.arg<lua::objpin<lua_bitmap>>(oidx);
				auto p = P.arg<lua::objpin<lua_palette>>(oidx + 1);
				e.erase();
				e.b = b;
				e.p = p;
			} else if(P.is_novalue(oidx)) {
				e.erase();
			} else
				P.expected("BITMAP, DBITMAP or nil", oidx);
			return 0;
		}
		int getsize(lua::state& L, lua::parameters& P)
		{
			L.pushnumber(width);
			L.pushnumber(height);
			return 2;
		}
		int getcsize(lua::state& L, lua::parameters& P)
		{
			L.pushnumber(cwidth);
			L.pushnumber(cheight);
			return 2;
		}
		size_t calcshift(size_t orig, int32_t shift, size_t dimension, size_t offset, bool circular)
		{
			if(circular) {
				orig -= offset;
				//Now the widow is scaled [0,dimension).
				if(shift >= 0)
					orig = (orig + shift) % dimension;
				else {
					orig += shift;
					while(orig > dimension) {
						//It overflowed.
						orig += dimension;
					}
				}
				orig += offset;
				return orig;
			} else
				return orig + shift;
		}
		int scroll(lua::state& L, lua::parameters& P)
		{
			int32_t ox, oy;
			size_t x0, y0, w, h;
			bool circx, circy;

			P(P.skipped(), ox, oy, P.optional(x0, 0), P.optional(y0, 0), P.optional(w, width),
				P.optional(h, height), P.optional(circx, false), P.optional(circy, false));

			threads::alock mh(lock);
			if(x0 > width || x0 + w > width || x0 + w < x0 || y0 > height || y0 + h > height ||
				y0 + h < y0)
				throw std::runtime_error("Scroll window out of range");
			if(!ox && !oy) return 0;
			tilemap_entry* tmp = tmpmap;
			for(size_t _y = 0; _y < h; _y++) {
				size_t y = _y + y0;
				size_t sy = calcshift(y, oy, h, y0, circy);
				if(sy < y0 || sy >= y0 + h)
					continue;
				for(size_t _x = 0; _x < w; _x++) {
					size_t x = _x + x0;
					size_t sx = calcshift(x, ox, w, x0, circx);
					if(sx < x0 || sx >= x0 + w)
						continue;
					else
						tmp[_y * w + _x] = map[sy * width + sx];
				}
			}
			for(size_t _y = 0; _y < h; _y++)
				for(size_t _x = 0; _x < w; _x++)
					map[(_y + y0) * width + (_x + x0)] = tmp[_y * w + _x];
			return 0;
		}
		std::string print()
		{
			return (stringfmt() << width << "*" << height << " (cell " << cwidth << "*" << cheight
				<< ")").str();
		}
		size_t width;
		size_t height;
		size_t cwidth;
		size_t cheight;
		tilemap_entry* map;
		tilemap_entry* tmpmap;
		threads::lock lock;
	};

	struct render_object_tilemap : public framebuffer::object
	{
		render_object_tilemap(int32_t _x, int32_t _y, int32_t _x0, int32_t _y0, uint32_t _w,
			uint32_t _h, bool _outside, lua::objpin<tilemap>& _map)
			: x(_x), y(_y), x0(_x0), y0(_y0), w(_w), h(_h), outside(_outside), map(_map) {}
		~render_object_tilemap() throw()
		{
		}
		bool kill_request(void* obj) throw()
		{
			return kill_request_ifeq(map.object(), obj);
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr) throw()
		{
			tilemap& _map = *map;
			threads::alock h(_map.lock);
			for(size_t ty = 0; ty < _map.height; ty++) {
				size_t basey = _map.cheight * ty;
				for(size_t tx = 0; tx < _map.width; tx++) {
					size_t basex = _map.cwidth * tx;
					composite_op(scr, _map.map[ty * _map.width + tx], basex, basey);
				}
			}
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr, int32_t xp,
			int32_t yp, const range& X, const range& Y, const range& sX, const range& sY,
			lua_dbitmap& d) throw()
		{
			if(!X.size() || !Y.size()) return;

			for(uint32_t r = Y.low(); r != Y.high(); r++) {
				typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + X.low();
				uint32_t xmin = X.low();
				bool cut = outside && sY.in(r);
				if(cut && sX.in(xmin)) {
					xmin = sX.high();
					//FIXME: This may overrun buffer (but the overrun pointer is not accessed.)
					eptr += (sX.high() - X.low());
				}
				for(uint32_t c = xmin; c < X.high(); c++, eptr++) {
					if(__builtin_expect(cut && c == sX.low(), 0)) {
						c += sX.size();
						eptr += sX.size();
					}
					d.pixels[r * d.width + c].apply(rptr[eptr]);
				}
			}
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr, int32_t xp,
			int32_t yp, const range& X, const range& Y, const range& sX, const range& sY, lua_bitmap& b,
			lua_palette& p) throw()
		{
			if(!X.size() || !Y.size()) return;

			p.palette_mutex.lock();
			framebuffer::color* palette = p.colors;
			size_t pallim = p.color_count;

			for(uint32_t r = Y.low(); r != Y.high(); r++) {
				typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + X.low();
				uint32_t xmin = X.low();
				bool cut = outside && sY.in(r);
				if(cut && sX.in(xmin)) {
					xmin = sX.high();
					//FIXME: This may overrun buffer (but the overrun pointer is not accessed.)
					eptr += (sX.high() - X.low());
				}
				for(uint32_t c = xmin; c < X.high(); c++, eptr++) {
					if(__builtin_expect(cut && c == sX.low(), 0)) {
						c += sX.size();
						eptr += sX.size();
					}
					uint16_t i = b.pixels[r * b.width + c];
					if(i < pallim)
						palette[i].apply(rptr[eptr]);
				}
			}
			p.palette_mutex.unlock();
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr, tilemap_entry& e, int32_t bx,
			int32_t by) throw()
		{
			size_t _w, _h;
			if(e.b) {
				_w = e.b->width;
				_h = e.b->height;
			} else if(e.d) {
				_w = e.d->width;
				_h = e.d->height;
			} else
				return;

			uint32_t oX = x + scr.get_origin_x() - x0;
			uint32_t oY = y + scr.get_origin_y() - y0;
			range bX = ((range::make_w(scr.get_width()) - oX) & range::make_s(bx, _w) &
				range::make_s(x0, w)) - bx;
			range bY = ((range::make_w(scr.get_height()) - oY) & range::make_s(by, _h) &
				range::make_s(y0, h)) - by;
			range sX = range::make_s(-x - bx + x0, scr.get_last_blit_width());
			range sY = range::make_s(-y - by + y0, scr.get_last_blit_height());

			if(e.b)
				composite_op(scr, oX + bx, oY + by, bX, bY, sX, sY, *e.b, *e.p);
			else if(e.d)
				composite_op(scr, oX + bx, oY + by, bX, bY, sX, sY, *e.d);
		}
		void operator()(struct framebuffer::fb<false>& x) throw() { composite_op(x); }
		void operator()(struct framebuffer::fb<true>& x) throw() { composite_op(x); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		int32_t x0;
		int32_t y0;
		uint32_t w;
		uint32_t h;
		bool outside;
		lua::objpin<tilemap> map;
	};

	template<bool outside> int tilemap::draw(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint32_t x, y, w, h;
		int32_t x0, y0;
		lua::objpin<tilemap> t;

		if(!core.lua2->render_ctx) return 0;

		P(t, x, y, P.optional(x0, 0), P.optional(y0, 0), P.optional(w, width * cwidth),
			P.optional(h, height * cheight));

		core.lua2->render_ctx->queue->create_add<render_object_tilemap>(x, y, x0, y0, w, h, outside, t);
		return 0;
	}

	int tilemap::create(lua::state& L, lua::parameters& P)
	{
		uint32_t w, h, px, py;

		P(w, h, px, py);

		lua::_class<tilemap>::create(L, w, h, px, py);
		return 1;
	}

	lua::_class<tilemap> LUA_class_tilemap(lua_class_gui, "TILEMAP", {
		{"new", tilemap::create},
	}, {
		{"draw", &tilemap::draw<false>},
		{"draw_outside", &tilemap::draw<true>},
		{"set", &tilemap::set},
		{"get", &tilemap::get},
		{"scroll", &tilemap::scroll},
		{"getsize", &tilemap::getsize},
		{"getcsize", &tilemap::getcsize},
	}, &tilemap::print);

	tilemap::tilemap(lua::state& L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight)
		: width(_width), height(_height), cwidth(_cwidth), cheight(_cheight)
	{
		if(overcommit(width, height, cwidth, cheight) / height / sizeof(tilemap_entry) < width)
			throw std::bad_alloc();

		map = lua::align_overcommit<tilemap, tilemap_entry>(this);
		tmpmap = &map[width * height];
		//Initialize the map!
		for(size_t i = 0; i < 2 * width * height; i++)
			new(map + i) tilemap_entry();
	}
}
