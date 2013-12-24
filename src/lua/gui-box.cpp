#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

namespace
{
	struct render_object_box : public framebuffer::object
	{
		render_object_box(int32_t _x, int32_t _y, int32_t _width, int32_t _height,
			framebuffer::color _outline1, framebuffer::color _outline2, framebuffer::color _fill,
			int32_t _thickness) throw()
			: x(_x), y(_y), width(_width), height(_height), outline1(_outline1), outline2(_outline2),
			fill(_fill), thickness(_thickness) {}
		~render_object_box() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			outline1.set_palette(scr);
			outline2.set_palette(scr);
			fill.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			int32_t xmin = 0;
			int32_t xmax = width;
			int32_t ymin = 0;
			int32_t ymax = height;
			framebuffer::clip_range(originx, scr.get_width(), x, xmin, xmax);
			framebuffer::clip_range(originy, scr.get_height(), y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer::fb<X>::element_t* rptr = scr.rowptr(y + r + originy);
				size_t eptr = x + xmin + originx;
				for(int32_t c = xmin; c < xmax; c++, eptr++)
					if((r < thickness && r <= (width - c)) || (c < thickness && c < (height - r)))
						outline1.apply(rptr[eptr]);
					else if(r < thickness || c < thickness || r >= height - thickness ||
						c >= width - thickness)
						outline2.apply(rptr[eptr]);
					else
						fill.apply(rptr[eptr]);
			}
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		int32_t width;
		int32_t height;
		framebuffer::color outline1;
		framebuffer::color outline2;
		framebuffer::color fill;
		int32_t thickness;
	};

	lua::fnptr gui_box(lua_func_misc, "gui.box", [](lua::state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint32_t thickness = 1;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		uint32_t width = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		uint32_t height = L.get_numeric_argument<uint32_t>(4, fname.c_str());
		L.get_numeric_argument<uint32_t>(5, thickness, fname.c_str());
		auto poutline1 = lua_get_fb_color(L, 6, fname, 0xFFFFFFU);
		auto poutline2 = lua_get_fb_color(L, 7, fname, 0x808080U);
		auto pfill = lua_get_fb_color(L, 8, fname, 0xC0C0C0U);
		lua_render_ctx->queue->create_add<render_object_box>(x, y, width, height, poutline1, poutline2,
			pfill, thickness);
		return 0;
	});
}
