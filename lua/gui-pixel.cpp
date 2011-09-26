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

	struct render_object_pixel : public render_object
	{
		render_object_pixel(int32_t _x, int32_t _y, uint16_t _color, uint8_t _alpha) throw();
		~render_object_pixel() throw();
		void operator()(struct screen& scr) throw();
	private:
		int32_t x;
		int32_t y;
		std::pair<uint32_t, uint32_t> color;
		uint8_t ialpha;
	};

	render_object_pixel::render_object_pixel(int32_t _x, int32_t _y, uint16_t _color, uint8_t _alpha) throw()
		: x(_x), y(_y)
	{
		ialpha = 32 - _alpha;
		color = premultipy_color(_color, _alpha);
	}

	void render_object_pixel::operator()(struct screen& scr) throw()
	{
		int32_t _x = x + scr.originx;
		int32_t _y = y + scr.originy;
		if(_x < 0 || static_cast<uint32_t>(_x) >= scr.width)
			return;
		if(_y < 0 || static_cast<uint32_t>(_y) >= scr.height)
			return;
		uint16_t& pixel = scr.rowptr(_y)[_x];
		pixel = blend(pixel, ialpha, color);
	}

	render_object_pixel::~render_object_pixel() throw()
	{
	}

	function_ptr_luafun gui_pixel("gui.pixel", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint16_t color = 0x7FFFU;
		uint8_t alpha = 32;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 3, color, fname.c_str());
		get_numeric_argument<uint8_t>(LS, 4, alpha, fname.c_str());
		lua_render_ctx->queue->add(*new render_object_pixel(x, y, color, alpha));
		return 0;
	});
}
