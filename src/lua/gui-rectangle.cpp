#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/range.hpp"
#include "library/lua-framebuffer.hpp"

namespace
{
	struct render_object_rectangle : public framebuffer::object
	{
		render_object_rectangle(int32_t _x, int32_t _y, int32_t _width, int32_t _height,
			framebuffer::color _outline, framebuffer::color _fill, int32_t _thickness) throw()
			: x(_x), y(_y), width(_width), height(_height), outline(_outline), fill(_fill),
			thickness(_thickness) {}
		~render_object_rectangle() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			uint32_t oX = x + scr.get_origin_x();
			uint32_t oY = y + scr.get_origin_y();
			range bX = (range::make_w(scr.get_width()) - oX) & range::make_w(width);
			range bY = (range::make_w(scr.get_height()) - oY) & range::make_w(height);
			for(uint32_t r = bY.low(); r != bY.high(); r++) {
				typename framebuffer::fb<X>::element_t* rptr = scr.rowptr(oY + r);
				size_t eptr = oX + bX.low();
				for(uint32_t c = bX.low(); c != bX.high(); c++, eptr++)
					if(r < thickness || c < thickness || r >= height - thickness ||
						c >= width - thickness)
						outline.apply(rptr[eptr]);
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
		uint32_t width;
		uint32_t height;
		framebuffer::color outline;
		framebuffer::color fill;
		uint32_t thickness;
	};

	int rectangle(lua::state& L, lua::parameters& P)
	{
		int32_t x, y;
		uint32_t width, height, thickness;
		framebuffer::color poutline, pfill;

		if(!lua_render_ctx) return 0;

		P(x, y, width, height, P.optional(thickness, 1), P.optional(poutline, 0xFFFFFFU),
			P.optional(pfill, -1));

		lua_render_ctx->queue->create_add<render_object_rectangle>(x, y, width, height, poutline, pfill,
			thickness);
		return 0;
	}

	int srectangle(lua::state& L, lua::parameters& P)
	{
		int32_t x, y;
		uint32_t width, height;
		framebuffer::color pcolor;

		if(!lua_render_ctx) return 0;

		P(x, y, width, height, P.optional(pcolor, 0xFFFFFFU));

		lua_render_ctx->queue->create_add<render_object_rectangle>(x, y, width, height, pcolor, pcolor, 0);
		return 0;
	}

	lua::functions rectangle_fns(lua_func_misc, "gui", {
		{"rectangle", rectangle},
		{"solidrectangle", srectangle},
	});
}
