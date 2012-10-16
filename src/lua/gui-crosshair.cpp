#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_crosshair : public render_object
	{
		render_object_crosshair(int32_t _x, int32_t _y, premultiplied_color _color, uint32_t _length) throw()
			: x(_x), y(_y), color(_color), length(_length) {}
		~render_object_crosshair() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			color.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			int32_t xmin = -static_cast<int32_t>(length);
			int32_t xmax = static_cast<int32_t>(length + 1);
			int32_t ymin = -static_cast<int32_t>(length);
			int32_t ymax = static_cast<int32_t>(length + 1);
			clip_range(originx, scr.get_width(), x, xmin, xmax);
			clip_range(originy, scr.get_height(), y, ymin, ymax);
			if(xmin <= 0 && xmax > 0)
				for(int32_t r = ymin; r < ymax; r++)
					color.apply(scr.rowptr(y + r + originy)[x + originx]);
			if(ymin <= 0 && ymax > 0)
				for(int32_t r = xmin; r < xmax; r++)
					color.apply(scr.rowptr(y + originy)[x + r + originx]);
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
	private:
		int32_t x;
		int32_t y;
		premultiplied_color color;
		uint32_t length;
	};

	function_ptr_luafun gui_crosshair(LS, "gui.crosshair", [](lua_state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t color = 0xFFFFFFU;
		uint32_t length = 10;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<uint32_t>(3, length, fname.c_str());
		L.get_numeric_argument<int64_t>(4, color, fname.c_str());
		premultiplied_color pcolor(color);
		lua_render_ctx->queue->create_add<render_object_crosshair>(x, y, pcolor, length);
		return 0;
	});
}
