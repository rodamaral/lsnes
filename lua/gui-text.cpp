#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_text : public render_object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, premultiplied_color _fg,
			premultiplied_color _bg) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg) {}
		~render_object_text() throw() {}
		void operator()(struct screen& scr) throw()
		{
			render_text(scr, x, y, text, fg, bg);
		}
	private:
		int32_t x;
		int32_t y;
		premultiplied_color fg;
		premultiplied_color bg;
		std::string text;
	};

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
		premultiplied_color fg(fgc, fga);
		premultiplied_color bg(bgc, bga);
		lua_render_ctx->queue->add(*new render_object_text(_x, _y, text, fg, bg));
		return 0;
	});
}
