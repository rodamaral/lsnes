#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_pixel : public render_object
	{
		render_object_pixel(int32_t _x, int32_t _y, premultiplied_color _color) throw()
			: x(_x), y(_y), color(_color) {}
		~render_object_pixel() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			color.set_palette(scr);
			int32_t _x = x + scr.get_origin_x();
			int32_t _y = y + scr.get_origin_y();
			if(_x < 0 || static_cast<uint32_t>(_x) >= scr.get_width())
				return;
			if(_y < 0 || static_cast<uint32_t>(_y) >= scr.get_height())
				return;
			color.apply(scr.rowptr(_y)[_x]);
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
	private:
		int32_t x;
		int32_t y;
		premultiplied_color color;
	};

	function_ptr_luafun gui_pixel(LS, "gui.pixel", [](lua_state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t color = 0xFFFFFFU;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<int64_t>(3, color, fname.c_str());
		premultiplied_color pcolor(color);
		lua_render_ctx->queue->create_add<render_object_pixel>(x, y, pcolor);
		return 0;
	});
}
