#include "lua/internal.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/png-decoder.hpp"
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
			b = NULL;
			d = NULL;
			p = NULL;
		}
		~tilemap_entry()
		{
			erase();
		}
		void erase()
		{
			delete b;
			delete d;
			delete p;
			b = NULL;
			d = NULL;
			p = NULL;
		}
		lua_obj_pin<lua_bitmap>* b;
		lua_obj_pin<lua_dbitmap>* d;
		lua_obj_pin<lua_palette>* p;
	};

	struct tilemap
	{
		tilemap(lua_State* L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight);
		~tilemap() 
		{
			umutex_class h(mutex);
			render_kill_request(this);
		}
		int draw(lua_State* L);
		int get(lua_State* L)
		{
			umutex_class h(mutex);
			uint32_t x = get_numeric_argument<uint32_t>(L, 2, "tilemap::get");
			uint32_t y = get_numeric_argument<uint32_t>(L, 3, "tilemap::get");
			if(x >= width || y >= height)
				return 0;
			tilemap_entry& e = map[y * width + x];
			if(e.b) {
				e.b->luapush(L);
				e.p->luapush(L);
				return 2;
			} else if(e.d) {
				e.d->luapush(L);
				return 1;
			} else
				return 0;
		}
		int set(lua_State* L)
		{
			umutex_class h(mutex);
			uint32_t x = get_numeric_argument<uint32_t>(L, 2, "tilemap::set");
			uint32_t y = get_numeric_argument<uint32_t>(L, 3, "tilemap::set");
			if(x >= width || y >= height)
				return 0;
			tilemap_entry& e = map[y * width + x];
			if(lua_class<lua_dbitmap>::is(L, 4)) {
				auto d = lua_class<lua_dbitmap>::pin(L, 4, "tilemap::set");
				e.erase();
				e.d = d;
			} else if(lua_class<lua_bitmap>::is(L, 4)) {
				auto b = lua_class<lua_bitmap>::pin(L, 4, "tilemap::set");
				auto p = lua_class<lua_palette>::pin(L, 5, "tilemap::set");
				e.erase();
				e.b = b;
				e.p = p;
			} else if(lua_type(L, 4) == LUA_TNIL || lua_type(L, 4) == LUA_TNONE) {
				e.erase();
			} else
				throw std::runtime_error("Expected BITMAP, DBITMAP or nil as argument 4 to "
					"tilemap::set");
			return 0;
		}
		int getsize(lua_State* L)
		{
			lua_pushnumber(L, width);
			lua_pushnumber(L, height);
			return 2;
		}
		int getcsize(lua_State* L)
		{
			lua_pushnumber(L, cwidth);
			lua_pushnumber(L, cheight);
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
		int scroll(lua_State* L)
		{
			umutex_class mh(mutex);
			int32_t ox = -get_numeric_argument<int32_t>(L, 2, "tilemap::scroll");
			int32_t oy = -get_numeric_argument<int32_t>(L, 3, "tilemap::scroll");
			size_t x0 = 0, y0 = 0, w = width, h = height;
			get_numeric_argument<size_t>(L, 4, x0, "tilemap::scroll");
			get_numeric_argument<size_t>(L, 5, y0, "tilemap::scroll");
			get_numeric_argument<size_t>(L, 6, w, "tilemap::scroll");
			get_numeric_argument<size_t>(L, 7, h, "tilemap::scroll");
			bool circx = (lua_type(L, 8) == LUA_TBOOLEAN && lua_toboolean(L, 8));
			bool circy = (lua_type(L, 9) == LUA_TBOOLEAN && lua_toboolean(L, 9));
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

	struct render_object_tilemap : public render_object
	{
		render_object_tilemap(int32_t _x, int32_t _y, int32_t _x0, int32_t _y0, uint32_t _w,
			uint32_t _h, lua_obj_pin<tilemap>* _map)
			: x(_x), y(_y), x0(_x0), y0(_y0), w(_w), h(_h), map(_map) {}
		~render_object_tilemap() throw()
		{
			delete map;
		}
		bool kill_request(void* obj) throw()
		{
			return kill_request_ifeq(unbox_any_pin(map), obj);
		}
		template<bool T> void composite_op(struct framebuffer<T>& scr) throw()
		{
			tilemap& _map = *map->object();
			umutex_class h(_map.mutex);
			for(size_t ty = 0; ty < _map.height; ty++) {
				size_t basey = _map.cheight * ty;
				for(size_t tx = 0; tx < _map.width; tx++) {
					size_t basex = _map.cwidth * tx;
					composite_op(scr, _map.map[ty * _map.width + tx], basex, basey);
				}
			}
		}
		template<bool T> void composite_op(struct framebuffer<T>& scr, int32_t xp,
			int32_t yp, int32_t xmin, int32_t xmax, int32_t ymin, int32_t ymax, lua_dbitmap& d) throw()
		{
			if(xmin >= xmax || ymin >= ymax) return;
			for(auto& c : d.pixels)
				c.set_palette(scr);

			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + xmin;
				for(int32_t c = xmin; c < xmax; c++, eptr++)
					d.pixels[r * d.width + c].apply(rptr[eptr]);
			}
		}
		template<bool T> void composite_op(struct framebuffer<T>& scr, int32_t xp,
			int32_t yp, int32_t xmin, int32_t xmax, int32_t ymin, int32_t ymax, lua_bitmap& b,
			lua_palette& p)
			throw()
		{
			if(xmin >= xmax || ymin >= ymax) return;
			p.palette_mutex->lock();
			premultiplied_color* palette = &p.colors[0];
			for(auto& c : p.colors)
				c.set_palette(scr);
			size_t pallim = p.colors.size();

			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer<T>::element_t* rptr = scr.rowptr(yp + r);
				size_t eptr = xp + xmin;
				for(int32_t c = xmin; c < xmax; c++, eptr++) {
					uint16_t i = b.pixels[r * b.width + c];
					if(i < pallim)
						palette[i].apply(rptr[eptr]);
				}
			}
			p.palette_mutex->unlock();
		}
		template<bool T> void composite_op(struct framebuffer<T>& scr, tilemap_entry& e, int32_t bx,
			int32_t by) throw()
		{
			size_t _w, _h;
			if(e.b) {
				_w = e.b->object()->width;
				_h = e.b->object()->height;
			} else if(e.d) {
				_w = e.d->object()->width;
				_h = e.d->object()->height;
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
				composite_op(scr, scrx, scry, xmin, xmax, ymin, ymax, *e.b->object(), *e.p->object());
			else if(e.d)
				composite_op(scr, scrx, scry, xmin, xmax, ymin, ymax, *e.d->object());
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
		void operator()(struct framebuffer<false>& x) throw() { composite_op(x); }
		void operator()(struct framebuffer<true>& x) throw() { composite_op(x); }
	private:
		int32_t x;
		int32_t y;
		int32_t x0;
		int32_t y0;
		uint32_t w;
		uint32_t h;
		lua_obj_pin<tilemap>* map;
	};

	int tilemap::draw(lua_State* L)
	{
		if(!lua_render_ctx)
			return 0;
		uint32_t x = get_numeric_argument<int32_t>(L, 2, "tilemap::draw");
		uint32_t y = get_numeric_argument<int32_t>(L, 3, "tilemap::draw");
		int32_t x0 = 0, y0 = 0;
		uint32_t w = width * cwidth, h = height * cheight;
		get_numeric_argument<int32_t>(L, 4, x0, "tilemap::draw");
		get_numeric_argument<int32_t>(L, 5, y0, "tilemap::draw");
		get_numeric_argument<uint32_t>(L, 6, w, "tilemap::draw");
		get_numeric_argument<uint32_t>(L, 7, h, "tilemap::draw");
		auto t = lua_class<tilemap>::pin(L, 1, "tilemap::draw");
		lua_render_ctx->queue->create_add<render_object_tilemap>(x, y, x0, y0, w, h, t);
		return 0;
	}

	function_ptr_luafun gui_ctilemap("gui.tilemap", [](lua_State* LS, const std::string& fname) -> int {
		uint32_t w = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		uint32_t h = get_numeric_argument<uint32_t>(LS, 2, fname.c_str());
		uint32_t px = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		uint32_t py = get_numeric_argument<uint32_t>(LS, 4, fname.c_str());
		tilemap* t = lua_class<tilemap>::create(LS, LS, w, h, px, py);
		return 1;
	});
}

DECLARE_LUACLASS(tilemap, "TILEMAP");

namespace
{
	tilemap::tilemap(lua_State* L, size_t _width, size_t _height, size_t _cwidth, size_t _cheight)
		: width(_width), height(_height), cwidth(_cwidth), cheight(_cheight)
	{
		static char done_key;
		if(lua_do_once(L, &done_key)) {
			objclass<tilemap>().bind(L, "draw", &tilemap::draw, true);
			objclass<tilemap>().bind(L, "set", &tilemap::set, true);
			objclass<tilemap>().bind(L, "get", &tilemap::get, true);
			objclass<tilemap>().bind(L, "scroll", &tilemap::scroll, true);
			objclass<tilemap>().bind(L, "getsize", &tilemap::getsize, true);
			objclass<tilemap>().bind(L, "getcsize", &tilemap::getcsize, true);
		}
		if(width * height / height != width)
			throw std::bad_alloc();
		map.resize(width * height);
	}
}
