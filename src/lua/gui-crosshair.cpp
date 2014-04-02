#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/range.hpp"

namespace
{
	struct render_object_crosshair : public framebuffer::object
	{
		render_object_crosshair(int32_t _x, int32_t _y, framebuffer::color _color, uint32_t _length) throw()
			: x(_x), y(_y), color(_color), length(_length) {}
		~render_object_crosshair() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			uint32_t oX = x + scr.get_origin_x();
			uint32_t oY = y + scr.get_origin_y();
			range bX = (range::make_w(scr.get_width()) - oX) & range::make_b(-length, length + 1);
			range bY = (range::make_w(scr.get_height()) - oY) & range::make_b(-length, length + 1);
			if(bX.in(0))
				for(uint32_t r = bY.low(); r != bY.high(); r++)
					color.apply(scr.rowptr(oY + r)[oX]);
			if(bY.in(0))
				for(uint32_t r = bX.low(); r != bX.high(); r++)
					color.apply(scr.rowptr(oY)[oX + r]);
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		framebuffer::color color;
		uint32_t length;
	};

	int crosshair(lua::state& L, lua::parameters& P)
	{
		int32_t x, y;
		uint32_t length;
		framebuffer::color pcolor;

		if(!lua_render_ctx) return 0;

		P(x, y, P.optional(length, 10), P.optional(pcolor, 0xFFFFFFU));

		lua_render_ctx->queue->create_add<render_object_crosshair>(x, y, pcolor, length);
		return 0;
	}

	lua::functions crosshair_fns(lua_func_misc, "gui", {
		{"crosshair", crosshair},
	});
}
