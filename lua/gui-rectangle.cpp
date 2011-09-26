#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_rectangle : public render_object
	{
		render_object_rectangle(int32_t _x, int32_t _y, uint32_t _width, uint32_t _height,
			premultiplied_color _outline, premultiplied_color _fill, uint32_t _thickness) throw()
			: x(_x), y(_y), width(_width), height(_height), outline(_outline), fill(_fill),
			thickness(_thickness) {}
		~render_object_rectangle() throw() {}
		void operator()(struct screen& scr) throw()
		{
			int32_t _x = x + scr.originx;
			int32_t _y = y + scr.originy;
			uint32_t xmin = 0;
			uint32_t xmax = width;
			if(_x < 0)
				xmin = static_cast<uint32_t>(-_x);
			if(xmin >= width)
				return;
			if(_x >= 0 && static_cast<uint32_t>(_x) >= scr.width)
				return;
			if(_x + width >= 0 && static_cast<uint32_t>(_x + width) >= scr.width)
				xmax = static_cast<uint32_t>(scr.width - _x);
			for(uint32_t i = 0; i < height; i++) {
				int32_t _y2 = i + _y;
				if(_y2 < 0 || _y2 >= scr.height)
					continue;
				uint16_t* rowbase = scr.rowptr(_y2) + (_x + xmin);
				for(uint32_t j = xmin; j < xmax; j++) {
					uint16_t& pix = rowbase[j - xmin];
					if(i < thickness || j < thickness || i >= height - thickness ||
						j >= width - thickness)
						outline.apply(pix);
					else
						fill.apply(pix);
				}
			}
		}
	private:
		int32_t x;
		int32_t y;
		uint32_t width;
		uint32_t height;
		premultiplied_color outline;
		premultiplied_color fill;
		uint32_t thickness;
	};

	function_ptr_luafun gui_rectangle("gui.rectangle", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint16_t outline = 0x7FFFU;
		uint8_t outlinealpha = 32;
		uint16_t fill = 0;
		uint8_t fillalpha = 0;
		uint32_t thickness = 1;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		uint32_t width = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		uint32_t height = get_numeric_argument<uint32_t>(LS, 4, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 5, thickness, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 6, outline, fname.c_str());
		get_numeric_argument<uint8_t>(LS, 7, outlinealpha, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 8, fill, fname.c_str());
		get_numeric_argument<uint8_t>(LS, 9, fillalpha, fname.c_str());
		premultiplied_color poutline(outline, outlinealpha);
		premultiplied_color pfill(fill, fillalpha);
		lua_render_ctx->queue->add(*new render_object_rectangle(x, y, width, height, poutline, pfill,
			thickness));
		return 0;
	});
}
