#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_text : public render_object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, uint16_t _fg = 0x7FFFU,
			uint8_t _fgalpha = 32, uint16_t _bg = 0, uint8_t _bgalpha = 0) throw(std::bad_alloc);
		~render_object_text() throw();
		void operator()(struct screen& scr) throw();
	private:
		int32_t x;
		int32_t y;
		uint16_t fg;
		uint8_t fgalpha;
		uint16_t bg;
		uint8_t bgalpha;
		std::string text;
	};

	render_object_text::render_object_text(int32_t _x, int32_t _y, const std::string& _text, uint16_t _fg,
		uint8_t _fgalpha, uint16_t _bg, uint8_t _bgalpha) throw(std::bad_alloc)
		: x(_x), y(_y), fg(_fg), fgalpha(_fgalpha), bg(_bg), bgalpha(_bgalpha), text(_text)
	{
	}

	void render_object_text::operator()(struct screen& scr) throw()
	{
		render_text(scr, x, y, text, fg, fgalpha, bg, bgalpha);
	}

	render_object_text::~render_object_text() throw()
	{
	}

	function_ptr_luafun gui_text("gui.text", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		uint32_t fgc = 0x7FFFU;
		uint32_t bgc = 0;
		uint16_t fga = 32;
		uint16_t bga = 0;
		int32_t _x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t _y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 4, fgc, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 5, fga, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 6, bgc, fname.c_str());
		get_numeric_argument<uint16_t>(LS, 7, bga, fname.c_str());
		std::string text = get_string_argument(LS, 3, fname.c_str());
		lua_render_ctx->queue->add(*new render_object_text(_x, _y, text, fgc, fga, bgc, bga));
		return 0;
	});
}
