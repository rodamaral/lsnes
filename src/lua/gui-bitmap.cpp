#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "lua/bitmap.hpp"
#include "library/threadtypes.hpp"
#include <vector>

lua_bitmap::lua_bitmap(uint32_t w, uint32_t h)
{
	width = w;
	height = h;
	pixels.resize(width * height);
	memset(&pixels[0], 0, width * height);
}

lua_dbitmap::lua_dbitmap(uint32_t w, uint32_t h)
{
	width = w;
	height = h;
	pixels.resize(width * height);
}

lua_palette::lua_palette()
{
}

lua_palette::~lua_palette()
{
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

		template<bool T> void composite_op(struct framebuffer<T>& scr) throw()
		{
			if(p)
				p->object()->palette_mutex.lock();
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
				p->object()->palette_mutex.unlock();
		}
		void operator()(struct framebuffer<false>& x) throw() { composite_op(x); }
		void operator()(struct framebuffer<true>& x) throw() { composite_op(x); }
		void clone(render_queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		lua_obj_pin<lua_bitmap>* b;
		lua_obj_pin<lua_dbitmap>* b2;
		lua_obj_pin<lua_palette>* p;
	};

	function_ptr_luafun gui_bitmap(LS, "gui.bitmap_draw", [](lua_state& L, const std::string& fname) -> int {
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
		} else {
			L.pushstring("Expected BITMAP or DBITMAP as argument 3 for gui.bitmap_draw.");
			L.error();
		}
		return 0;
	});

	function_ptr_luafun gui_cpalette(LS, "gui.palette_new", [](lua_state& L, const std::string& fname) -> int {
		lua_class<lua_palette>::create(L);
		return 1;
	});

	function_ptr_luafun gui_cbitmap(LS, "gui.bitmap_new", [](lua_state& L, const std::string& fname) -> int {
		uint32_t w = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		uint32_t h = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		bool d = L.get_bool(3, fname.c_str());
		if(d) {
			int64_t c = -1;
			L.get_numeric_argument<int64_t>(4, c, fname.c_str());
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = premultiplied_color(c);
		} else {
			uint16_t c = 0;
			L.get_numeric_argument<uint16_t>(4, c, fname.c_str());
			lua_bitmap* b = lua_class<lua_bitmap>::create(L, w, h);
			for(size_t i = 0; i < b->width * b->height; i++)
				b->pixels[i] = c;
		}
		return 1;
	});

	function_ptr_luafun gui_epalette(LS, "gui.palette_set", [](lua_state& L, const std::string& fname) -> int {
		lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
		uint16_t c = L.get_numeric_argument<uint16_t>(2, fname.c_str());
		int64_t nval = L.get_numeric_argument<int64_t>(3, fname.c_str());
		premultiplied_color nc(nval);
		//The mutex lock protects only the internals of colors array.
		if(p->colors.size() <= c) {
			p->palette_mutex.lock();
			p->colors.resize(static_cast<uint32_t>(c) + 1);
			p->palette_mutex.unlock();
		}
		p->colors[c] = nc;
		return 0;
	});

	function_ptr_luafun pset_bitmap(LS, "gui.bitmap_pset", [](lua_state& L, const std::string& fname) -> int {
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
			b->pixels[y * b->width + x] = premultiplied_color(c);
		} else {
			L.pushstring("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_pset.");
			L.error();
		}
		return 0;
	});

	function_ptr_luafun size_bitmap(LS, "gui.bitmap_size", [](lua_state& L, const std::string& fname) -> int {
		if(lua_class<lua_bitmap>::is(L, 1)) {
			lua_bitmap* b = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			L.pushnumber(b->width);
			L.pushnumber(b->height);
		} else if(lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			L.pushnumber(b->width);
			L.pushnumber(b->height);
		} else {
			L.pushstring("Expected BITMAP or DBITMAP as argument 1 for gui.bitmap_size.");
			L.error();
		}
		return 2;
	});

	function_ptr_luafun blit_bitmap(LS, "gui.bitmap_blit", [](lua_state& L, const std::string& fname) -> int {
		uint32_t dx = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		uint32_t dy = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		uint32_t sx = L.get_numeric_argument<uint32_t>(5, fname.c_str());
		uint32_t sy = L.get_numeric_argument<uint32_t>(6, fname.c_str());
		uint32_t w = L.get_numeric_argument<uint32_t>(7, fname.c_str());
		uint32_t h = L.get_numeric_argument<uint32_t>(8, fname.c_str());
		int64_t ck = 0x100000000ULL;
		L.get_numeric_argument<int64_t>(9, ck, fname.c_str());
		bool nck = false;
		premultiplied_color pck(ck);
		uint32_t ckorig = pck.orig;
		uint16_t ckoriga = pck.origa;
		if(ck == 0x100000000ULL)
			nck = true;
		if(lua_class<lua_bitmap>::is(L, 1) && lua_class<lua_bitmap>::is(L, 4)) {
			lua_bitmap* db = lua_class<lua_bitmap>::get(L, 1, fname.c_str());
			lua_bitmap* sb = lua_class<lua_bitmap>::get(L, 4, fname.c_str());
			while((dx + w > db->width || sx + w > sb->width) && w > 0)
				w--;
			while((dy + h > db->height || sy + h > sb->height) && h > 0)
				h--;
			size_t sidx = sy * sb->width + sx;
			size_t didx = dy * db->width + dx;
			size_t srskip = sb->width - w;
			size_t drskip = db->width - w;
			for(uint32_t j = 0; j < h; j++) {
				for(uint32_t i = 0; i < w; i++) {
					uint16_t pix = sb->pixels[sidx];
					if(pix != ck)	//No need to check nck, as that value is out of range.
						db->pixels[didx] = pix;
					sidx++;
					didx++;
				}
				sidx += srskip;
				didx += drskip;
			}
		} else if(lua_class<lua_dbitmap>::is(L, 1) && lua_class<lua_dbitmap>::is(L, 1)) {
			lua_dbitmap* db = lua_class<lua_dbitmap>::get(L, 1, fname.c_str());
			lua_dbitmap* sb = lua_class<lua_dbitmap>::get(L, 4, fname.c_str());
			while((dx + w > db->width || sx + w > sb->width) && w > 0)
				w--;
			while((dy + h > db->height || sy + h > sb->height) && h > 0)
				h--;
			size_t sidx = sy * sb->width + sx;
			size_t didx = dy * db->width + dx;
			size_t srskip = sb->width - w;
			size_t drskip = db->width - w;
			for(uint32_t j = 0; j < h; j++) {
				for(uint32_t i = 0; i < w; i++) {
					premultiplied_color pix = sb->pixels[sidx];
					if(pix.orig != ckorig || pix.origa != ckoriga || nck)
						db->pixels[didx] = pix;
					sidx++;
					didx++;
				}
				sidx += srskip;
				didx += drskip;
			}
		} else {
			L.pushstring("Expected BITMAP or DBITMAP as arguments 1&4 for gui.bitmap_pset.");
			L.error();
		}
		return 0;
	});

	function_ptr_luafun gui_loadbitmap(LS, "gui.bitmap_load", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		auto bitmap = lua_loaded_bitmap::load(name);
		if(bitmap.d) {
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, bitmap.w, bitmap.h);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = premultiplied_color(bitmap.bitmap[i]);
			return 1;
		} else {
			lua_bitmap* b = lua_class<lua_bitmap>::create(L, bitmap.w, bitmap.h);
			lua_palette* p = lua_class<lua_palette>::create(L);
			for(size_t i = 0; i < bitmap.w * bitmap.h; i++)
				b->pixels[i] = bitmap.bitmap[i];
			p->colors.resize(bitmap.palette.size());
			for(size_t i = 0; i < bitmap.palette.size(); i++)
				p->colors[i] = premultiplied_color(bitmap.palette[i]);
			return 2;
		}
	});

	function_ptr_luafun gui_dpalette(LS, "gui.palette_debug", [](lua_state& L, const std::string& fname) -> int {
		lua_palette* p = lua_class<lua_palette>::get(L, 1, fname.c_str());
		size_t i = 0;
		for(auto c : p->colors)
			messages << "Color #" << (i++) << ": " << c.orig << ":" << c.origa << std::endl;
		return 0;
	});
}

DECLARE_LUACLASS(lua_palette, "PALETTE");
DECLARE_LUACLASS(lua_bitmap, "BITMAP");
DECLARE_LUACLASS(lua_dbitmap, "DBITMAP");
