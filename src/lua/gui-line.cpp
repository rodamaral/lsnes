#include "lua/internal.hpp"
#include "core/render.hpp"

namespace
{
	struct render_object_line : public render_object
	{
		render_object_line(int32_t _x1, int32_t _x2, int32_t _y1, int32_t _y2, premultiplied_color _color)
			throw()
			: x1(_x1), y1(_y1), x2(_x2), y2(_y2), color(_color) {}
		~render_object_line() throw() {}
		template<bool X> void op(struct screen<X>& scr) throw()
		{
			color.set_palette(scr);
			int32_t xdiff = x2 - x1;
			int32_t ydiff = y2 - y1;
			if(xdiff < 0)
				xdiff = -xdiff;
			if(ydiff < 0)
				ydiff = -ydiff;
			if(xdiff >= ydiff) {
				//X-major line.
				if(x2 < x1) {
					//Swap points so that x1 < x2.
					std::swap(x1, x2);
					std::swap(y1, y2);
				}
				//The slope of the line is (y2 - y1) / (x2 - x1) = +-ydiff / xdiff
				int32_t y = y1;
				int32_t ysub = 0;
				for(int32_t x = x1; x <= x2; x++) {
					if(x < 0 || static_cast<uint32_t>(x) >= scr.width)
						goto nodraw1;
					if(y < 0 || static_cast<uint32_t>(y) >= scr.height)
						goto nodraw1;
					color.apply(scr.rowptr(y)[x]);
nodraw1:
					ysub += ydiff;
					if(ysub >= xdiff) {
						ysub -= xdiff;
						if(y2 > y1)
							y++;
						else
							y--;
					}
				}
			} else {
				//Y-major line.
				if(x2 < x1) {
					//Swap points so that y1 < y2.
					std::swap(x1, x2);
					std::swap(y1, y2);
				}
				//The slope of the line is (x2 - x1) / (y2 - y1) = +-xdiff / ydiff
				int32_t x = x1;
				int32_t xsub = 0;
				for(int32_t y = y1; y <= y2; y++) {
					if(x < 0 || static_cast<uint32_t>(x) >= scr.width)
						goto nodraw2;
					if(y < 0 || static_cast<uint32_t>(y) >= scr.height)
						goto nodraw2;
					color.apply(scr.rowptr(y)[x]);
nodraw2:
					xsub += xdiff;
					if(xsub >= ydiff) {
						xsub -= ydiff;
						if(x2 > x1)
							x++;
						else
							x--;
					}
				}
			}
		}
		void operator()(struct screen<true>& scr) throw()  { op(scr); }
		void operator()(struct screen<false>& scr) throw() { op(scr); }
	private:
		int32_t x1;
		int32_t y1;
		int32_t x2;
		int32_t y2;
		premultiplied_color color;
	};

	function_ptr_luafun gui_pixel("gui.line", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t color = 0xFFFFFFU;
		int32_t x1 = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y1 = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		int32_t x2 = get_numeric_argument<int32_t>(LS, 3, fname.c_str());
		int32_t y2 = get_numeric_argument<int32_t>(LS, 4, fname.c_str());
		get_numeric_argument<int64_t>(LS, 5, color, fname.c_str());
		premultiplied_color pcolor(color);
		lua_render_ctx->queue->add(*new render_object_line(x1, x2, y1, y2, pcolor));
		return 0;
	});
}
