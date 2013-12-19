#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/png-codec.hpp"
#include "library/string.hpp"
#include "library/threadtypes.hpp"
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
		lua_obj_pin<lua_bitmap> b;
		lua_obj_pin<lua_dbitmap> d;
		lua_obj_pin<lua_palette> p;
	};

	struct tilemap
	{
		tilemap(lua_state& L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight);
		~tilemap()
		{
			umutex_class h(mutex);
			render_kill_request(this);
		}
		int draw(lua_state& L, const std::string& fname);
		int get(lua_state& L, const std::string& fname)
		{
			umutex_class h(mutex);
			uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
			uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
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
		int set(lua_state& L, const std::string& fname)
		{
			umutex_class h(mutex);
			uint32_t x = L.get_numeric_argument<uint32_t>(2, fname.c_str());
			uint32_t y = L.get_numeric_argument<uint32_t>(3, fname.c_str());
			if(x >= width || y >= height)
				return 0;
			tilemap_entry& e = map[y * width + x];
			if(lua_class<lua_dbitmap>::is(L, 4)) {
				auto d = lua_class<lua_dbitmap>::pin(L, 4, fname.c_str());
				e.erase();
				e.d = d;
			} else if(lua_class<lua_bitmap>::is(L, 4)) {
				auto b = lua_class<lua_bitmap>::pin(L, 4, fname.c_str());
				auto p = lua_class<lua_palette>::pin(L, 5, fname.c_str());
				e.erase();
				e.b = b;
				e.p = p;
			} else if(L.type(4) == LUA_TNIL || L.type(4) == LUA_TNONE) {
				e.erase();
			} else
				throw std::runtime_error("Expected BITMAP, DBITMAP or nil as argument 4 to "
					+ fname);
			return 0;
		}
		int getsize(lua_state& L, const std::string& fname)
		{
			L.pushnumber(width);
			L.pushnumber(height);
			return 2;
		}
		int getcsize(lua_state& L, const std::string& fname)
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
		int scroll(lua_state& L, const std::string& fname)
		{
			umutex_class mh(mutex);
			int32_t ox = -L.get_numeric_argument<int32_t>(2, fname.c_str());
			int32_t oy = -L.get_numeric_argument<int32_t>(3, fname.c_str());
			size_t x0 = 0, y0 = 0, w = width, h = height;
			L.get_numeric_argument<size_t>(4, x0, fname.c_str());
			L.get_numeric_argument<size_t>(5, y0, fname.c_str());
			L.get_numeric_argument<size_t>(6, w, fname.c_str());
			L.get_numeric_argument<size_t>(7, h, fname.c_str());
			bool circx = (L.type(8) == LUA_TBOOLEAN && L.toboolean(8));
			bool circy = (L.type(9) == LUA_TBOOLEAN && L.toboolean(9));
			if(x0 > width || x0 + w > width || x0 + w < x0 || y0 > height || y0 + h > height ||
				y0 + h < y0)
				throw std::runtime_error("Scroll window out of range");
			if(!ox && !oy) return 0;
			std::vector<tilemap_entry> tmp;
			tmp.resize(w * h);
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
		std::vector<tilemap_entry> map;
		mutex_class mutex;
	};

	struct render_object_tilemap : public framebuffer::object
	{
		render_object_tilemap(int32_t _x, int32_t _y, int32_t _x0, int32_t _y0, uint32_t _w,
			uint32_t _h, lua_obj_pin<tilemap> _map)
			: x(_x), y(_y), x0(_x0), y0(_y0), w(_w), h(_h), map(_map) {}
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
			umutex_class h(_map.mutex);
			for(size_t ty = 0; ty < _map.height; ty++) {
				size_t basey = _map.cheight * ty;
				for(size_t tx = 0; tx < _map.width; tx++) {
					size_t basex = _map.cwidth * tx;
					composite_op(scr, _map.map[ty * _map.width + tx], basex, basey);
				}
			}
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr, int32_t xp,
			int32_t yp, int32_t xmin, int32_t xmax, int32_t ymin, int32_t ymax, lua_dbitmap& d) throw()
		{
			if(xmin >= xmax || ymin >= ymax) return;
			for(auto& c : d.pixels)
				c.set_palette(scr);

			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + xmin;
				for(int32_t c = xmin; c < xmax; c++, eptr++)
					d.pixels[r * d.width + c].apply(rptr[eptr]);
			}
		}
		template<bool T> void composite_op(struct framebuffer::fb<T>& scr, int32_t xp,
			int32_t yp, int32_t xmin, int32_t xmax, int32_t ymin, int32_t ymax, lua_bitmap& b,
			lua_palette& p)
			throw()
		{
			if(xmin >= xmax || ymin >= ymax) return;
			p.palette_mutex.lock();
			framebuffer::color* palette = &p.colors[0];
			for(auto& c : p.colors)
				c.set_palette(scr);
			size_t pallim = p.colors.size();

			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer::fb<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + xmin;
				for(int32_t c = xmin; c < xmax; c++, eptr++) {
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
			//Calculate notional screen coordinates for the tile.
			int32_t scrx = x + scr.get_origin_x() + bx - x0;
			int32_t scry = y + scr.get_origin_y() + by - y0;
			int32_t scrw = scr.get_width();
			int32_t scrh = scr.get_height();
			int32_t xmin = 0;
			int32_t xmax = _w;
			int32_t ymin = 0;
			int32_t ymax = _h;
			clip(scrx, scrw, x + scr.get_origin_x(), w, xmin, xmax);
			clip(scry, scrh, y + scr.get_origin_y(), h, ymin, ymax);
			if(e.b)
				composite_op(scr, scrx, scry, xmin, xmax, ymin, ymax, *e.b, *e.p);
			else if(e.d)
				composite_op(scr, scrx, scry, xmin, xmax, ymin, ymax, *e.d);
		}
		//scrc + cmin >= 0 and scrc + cmax <= scrd  (Clip on screen).
		//scrc + cmin >= bc and scrc + cmax <= bc + d  (Clip on texture).
		void clip(int32_t scrc, int32_t scrd, int32_t bc, int32_t d, int32_t& cmin, int32_t& cmax)
		{
			if(scrc + cmin < 0)
				cmin = -scrc;
			if(scrc + cmax > scrd)
				cmax = scrd - scrc;
			if(scrc + cmin < bc)
				cmin = bc - scrc;
			if(scrc + cmax > bc + d)
				cmax = bc + d - scrc;
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
		lua_obj_pin<tilemap> map;
	};

	int tilemap::draw(lua_state& L, const std::string& fname)
	{
		if(!lua_render_ctx)
			return 0;
		uint32_t x = L.get_numeric_argument<int32_t>(2, fname.c_str());
		uint32_t y = L.get_numeric_argument<int32_t>(3, fname.c_str());
		int32_t x0 = 0, y0 = 0;
		uint32_t w = width * cwidth, h = height * cheight;
		L.get_numeric_argument<int32_t>(4, x0, fname.c_str());
		L.get_numeric_argument<int32_t>(5, y0, fname.c_str());
		L.get_numeric_argument<uint32_t>(6, w, fname.c_str());
		L.get_numeric_argument<uint32_t>(7, h, fname.c_str());
		auto t = lua_class<tilemap>::pin(L, 1, fname.c_str());
		lua_render_ctx->queue->create_add<render_object_tilemap>(x, y, x0, y0, w, h, t);
		return 0;
	}

	function_ptr_luafun gui_ctilemap(lua_func_misc, "gui.tilemap", [](lua_state& LS, const std::string& fname) ->
		int {
		uint32_t w = LS.get_numeric_argument<uint32_t>(1, fname.c_str());
		uint32_t h = LS.get_numeric_argument<uint32_t>(2, fname.c_str());
		uint32_t px = LS.get_numeric_argument<uint32_t>(3, fname.c_str());
		uint32_t py = LS.get_numeric_argument<uint32_t>(4, fname.c_str());
		tilemap* t = lua_class<tilemap>::create(LS, w, h, px, py);
		return 1;
	});
}

DECLARE_LUACLASS(tilemap, "TILEMAP");

namespace
{
	tilemap::tilemap(lua_state& L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight)
		: width(_width), height(_height), cwidth(_cwidth), cheight(_cheight)
	{
		objclass<tilemap>().bind_multi(L, {
			{"draw", &tilemap::draw},
			{"set", &tilemap::set},
			{"get", &tilemap::get},
			{"scroll", &tilemap::scroll},
			{"getsize", &tilemap::getsize},
			{"getcsize", &tilemap::getcsize},
		});
		if(width * height / height != width)
			throw std::bad_alloc();
		map.resize(width * height);
	}
}
