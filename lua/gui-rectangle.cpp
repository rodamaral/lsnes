#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	std::pair<uint32_t, uint32_t> premultipy_color(uint16_t color, uint8_t alpha)
	{
		uint32_t a, b;
		a = color & 0x7C1F;
		b = color & 0x03E0;
		return std::make_pair(a * alpha, b * alpha);
	}

	inline uint16_t blend(uint16_t orig, uint8_t ialpha, std::pair<uint32_t, uint32_t> with) throw()
	{
		uint32_t a, b;
		a = orig & 0x7C1F;
		b = orig & 0x03E0;
		return (((a * ialpha + with.first) >> 5) & 0x7C1F) | (((b * ialpha + with.second) >> 5) & 0x03E0);
	}

	struct render_object_rectangle : public render_object
	{
		render_object_rectangle(int32_t _x, int32_t _y, uint32_t _width, uint32_t _height,
			uint16_t _outline, uint8_t _outlinealpha, uint16_t _fill, uint8_t _fillalpha, 
			uint32_t _thickness) throw();
		~render_object_rectangle() throw();
		void operator()(struct screen& scr) throw();
	private:
		int32_t x;
		int32_t y;
		uint32_t width;
		uint32_t height;
		std::pair<uint32_t, uint32_t> outline;
		uint8_t ioutlinealpha;
		std::pair<uint32_t, uint32_t> fill;
		uint8_t ifillalpha;
		uint32_t thickness;
	};

	render_object_rectangle::render_object_rectangle(int32_t _x, int32_t _y, uint32_t _width, uint32_t _height,
			uint16_t _outline, uint8_t _outlinealpha, uint16_t _fill, uint8_t _fillalpha, 
			uint32_t _thickness) throw()
		: x(_x), y(_y), width(_width), height(_height), thickness(_thickness)
	{
		ioutlinealpha = 32 - _outlinealpha;
		ifillalpha = 32 - _fillalpha;
		outline = premultipy_color(_outline, _outlinealpha);
		fill = premultipy_color(_fill, _fillalpha);
	}

	void render_object_rectangle::operator()(struct screen& scr) throw()
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
				if(i < thickness || j < thickness || i >= height - thickness || j >= width - thickness)
					pix = blend(pix, ioutlinealpha, outline);
				else
					pix = blend(pix, ifillalpha, fill);
			}
		}
	}

	render_object_rectangle::~render_object_rectangle() throw()
	{
	}

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
		lua_render_ctx->queue->add(*new render_object_rectangle(x, y, width, height, outline, outlinealpha,
			fill, fillalpha, thickness));
		return 0;
	});
}
