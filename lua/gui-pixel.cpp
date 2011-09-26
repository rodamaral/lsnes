#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_pixel : public render_object
	{
		render_object_pixel(int32_t _x, int32_t _y, premultiplied_color _color) throw()
			: x(_x), y(_y), color(_color) {}
		~render_object_pixel() throw() {}
		void operator()(struct screen& scr) throw()
		{
			int32_t _x = x + scr.originx;
			int32_t _y = y + scr.originy;
			if(_x < 0 || static_cast<uint32_t>(_x) >= scr.width)
				return;
			if(_y < 0 || static_cast<uint32_t>(_y) >= scr.height)
				return;
			color.apply(scr.rowptr(_y)[_x]);
		}
	private:
		int32_t x;
		int32_t y;
		premultiplied_color color;
	};

	function_ptr_luafun gui_pixel("gui.pixel", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint16_t color = 0x7FFFU;
		uint8_t alpha = 32;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 3, color, fname.c_str());
		get_numeric_argument<uint8_t>(LS, 4, alpha, fname.c_str());
		premultiplied_color pcolor(color, alpha);
		lua_render_ctx->queue->add(*new render_object_pixel(x, y, pcolor));
		return 0;
	});
}
