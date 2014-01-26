#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

namespace
{
	struct render_object_crosshair : public framebuffer::object
	{
		render_object_crosshair(int32_t _x, int32_t _y, framebuffer::color _color, uint32_t _length) throw()
			: x(_x), y(_y), color(_color), length(_length) {}
		~render_object_crosshair() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			color.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			int32_t xmin = -static_cast<int32_t>(length);
			int32_t xmax = static_cast<int32_t>(length + 1);
			int32_t ymin = -static_cast<int32_t>(length);
			int32_t ymax = static_cast<int32_t>(length + 1);
			framebuffer::clip_range(originx, scr.get_width(), x, xmin, xmax);
			framebuffer::clip_range(originy, scr.get_height(), y, ymin, ymax);
			if(xmin <= 0 && xmax > 0)
				for(int32_t r = ymin; r < ymax; r++)
					color.apply(scr.rowptr(y + r + originy)[x + originx]);
			if(ymin <= 0 && ymax > 0)
				for(int32_t r = xmin; r < xmax; r++)
					color.apply(scr.rowptr(y + originy)[x + r + originx]);
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

	lua::fnptr2 gui_crosshair(lua_func_misc, "gui.crosshair", [](lua::state& L, lua::parameters& P)
		-> int {
		if(!lua_render_ctx)
			return 0;
		auto x = P.arg<int32_t>();
		auto y = P.arg<int32_t>();
		auto length = P.arg_opt<uint32_t>(10);
		auto pcolor = P.color(0xFFFFFFU);
		lua_render_ctx->queue->create_add<render_object_crosshair>(x, y, pcolor, length);
		return 0;
	});
}
