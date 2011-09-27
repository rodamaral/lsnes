#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_crosshair : public render_object
	{
		render_object_crosshair(int32_t _x, int32_t _y, premultiplied_color _color, uint32_t _length) throw()
			: x(_x), y(_y), color(_color), length(_length) {}
		~render_object_crosshair() throw() {}
		void operator()(struct screen& scr) throw()
		{
			int32_t xmin = -static_cast<int32_t>(length);
			int32_t xmax = static_cast<int32_t>(length + 1);
			int32_t ymin = -static_cast<int32_t>(length);
			int32_t ymax = static_cast<int32_t>(length + 1);
			clip_range(scr.originx, scr.width, x, xmin, xmax);
			clip_range(scr.originy, scr.height, y, ymin, ymax);
			if(xmin <= 0 && xmax > 0)
				for(int32_t r = ymin; r < ymax; r++)
					color.apply(scr.rowptr(y + r + scr.originy)[x + scr.originx]);
			if(ymin <= 0 && ymax > 0)
				for(int32_t r = xmin; r < xmax; r++)
					color.apply(scr.rowptr(y + scr.originy)[x + r + scr.originx]);
		}
	private:
		int32_t x;
		int32_t y;
		premultiplied_color color;
		uint32_t length;
	};

	function_ptr_luafun gui_crosshair("gui.crosshair", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint16_t color = 0x7FFFU;
		uint8_t alpha = 32;
		uint32_t length = 10;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 3, length, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 4, color, fname.c_str());
		get_numeric_argument<uint8_t>(LS, 5, alpha, fname.c_str());
		premultiplied_color pcolor(color, alpha);
		lua_render_ctx->queue->add(*new render_object_crosshair(x, y, pcolor, length));
		return 0;
	});
}
