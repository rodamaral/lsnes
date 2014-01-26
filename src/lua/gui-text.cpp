#include "lua/internal.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

namespace
{
	struct render_object_text : public framebuffer::object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, framebuffer::color _fg,
			framebuffer::color _bg, bool _hdbl = false, bool _vdbl = false) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hdbl(_hdbl), vdbl(_vdbl) {}
		~render_object_text() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			main_font.render(scr, x, y, text, fg, bg, hdbl, vdbl);
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		std::string text;
		framebuffer::color fg;
		framebuffer::color bg;
		bool hdbl;
		bool vdbl;
	};

	template<bool hdbl, bool vdbl>
	int internal_gui_text(lua::state& L, lua::parameters& P)
	{
		if(!lua_render_ctx)
			return 0;
		auto _x = P.arg<int32_t>();
		auto _y = P.arg<int32_t>();
		auto text = P.arg<std::string>();
		auto fg = P.color(0xFFFFFFU);
		auto bg = P.color(-1);
		lua_render_ctx->queue->create_add<render_object_text>(_x, _y, text, fg, bg, hdbl, vdbl);
		return 0;
	}

	lua::fnptr2 gui_text(lua_func_misc, "gui.text", internal_gui_text<false, false>);
	lua::fnptr2 gui_textH(lua_func_misc, "gui.textH", internal_gui_text<true, false>);
	lua::fnptr2 gui_textV(lua_func_misc, "gui.textV", internal_gui_text<false, true>);
	lua::fnptr2 gui_textHV(lua_func_misc, "gui.textHV", internal_gui_text<true, true>);
}
