#include "lua-int.hpp"
#include "render.hpp"

namespace
{
	struct render_object_text : public render_object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, uint32_t _fg = 0xFFFFFFFFU,
			uint16_t _fgalpha = 255, uint32_t _bg = 0, uint16_t _bgalpha = 0) throw(std::bad_alloc);
		~render_object_text() throw();
		void operator()(struct screen& scr) throw();
	private:
		int32_t x;
		int32_t y;
		uint32_t fg;
		uint16_t fgalpha;
		uint32_t bg;
		uint16_t bgalpha;
		std::string text;
	};

	render_object_text::render_object_text(int32_t _x, int32_t _y, const std::string& _text, uint32_t _fg,
		uint16_t _fgalpha, uint32_t _bg, uint16_t _bgalpha) throw(std::bad_alloc)
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

	class lua_gui_text : public lua_function
	{
	public:
		lua_gui_text() : lua_function("gui.text") {}
		int invoke(lua_State* LS, window* win)
		{
			if(!lua_render_ctx)
				return 0;
			uint32_t x255 = 255;
			uint32_t fgc = (x255 << lua_render_ctx->rshift) | (x255 << lua_render_ctx->gshift) |
				(x255 << lua_render_ctx->bshift);
			uint32_t bgc = 0;
			uint16_t fga = 256;
			uint16_t bga = 0;
			int32_t _x = get_numeric_argument<int32_t>(LS, 1, "gui.text");
			int32_t _y = get_numeric_argument<int32_t>(LS, 2, "gui.text");
			get_numeric_argument<uint32_t>(LS, 4, fgc, "gui.text");
			get_numeric_argument<uint16_t>(LS, 5, fga, "gui.text");
			get_numeric_argument<uint32_t>(LS, 6, bgc, "gui.text");
			get_numeric_argument<uint16_t>(LS, 7, bga, "gui.text");
			std::string text = get_string_argument(LS, 3, "gui.text");
			lua_render_ctx->queue->add(*new render_object_text(_x, _y, text, fgc, fga, bgc, bga));
			return 0;
		}
	} gui_text;
}
