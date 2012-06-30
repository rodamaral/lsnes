#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_box : public render_object
	{
		render_object_box(int32_t _x, int32_t _y, uint32_t _width, uint32_t _height,
			premultiplied_color _outline1, premultiplied_color _outline2, premultiplied_color _fill,
			uint32_t _thickness) throw()
			: x(_x), y(_y), width(_width), height(_height), outline1(_outline1), outline2(_outline2),
			fill(_fill), thickness(_thickness) {}
		~render_object_box() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
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
			clip_range(originx, scr.get_width(), x, xmin, xmax);
			clip_range(originy, scr.get_height(), y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				typename framebuffer<X>::element_t* rptr = scr.rowptr(y + r + originy);
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
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
	private:
		int32_t x;
		int32_t y;
		uint32_t width;
		uint32_t height;
		premultiplied_color outline1;
		premultiplied_color outline2;
		premultiplied_color fill;
		uint32_t thickness;
	};

	function_ptr_luafun gui_box("gui.box", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t outline1 = 0xFFFFFFU;
		int64_t outline2 = 0x808080U;
		int64_t fill = 0xC0C0C0U;
		uint32_t thickness = 1;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		uint32_t width = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		uint32_t height = get_numeric_argument<uint32_t>(LS, 4, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 5, thickness, fname.c_str());
		get_numeric_argument<int64_t>(LS, 6, outline1, fname.c_str());
		get_numeric_argument<int64_t>(LS, 7, outline2, fname.c_str());
		get_numeric_argument<int64_t>(LS, 8, fill, fname.c_str());
		premultiplied_color poutline1(outline1);
		premultiplied_color poutline2(outline2);
		premultiplied_color pfill(fill);
		lua_render_ctx->queue->create_add<render_object_box>(x, y, width, height, poutline1, poutline2,
			pfill, thickness);
		return 0;
	});
}
