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
		int32_t x, y;
		std::string text;
		framebuffer::color fg, bg;

		if(!lua_render_ctx) return 0;

		P(x, y, text, P.optional(fg, 0xFFFFFFU), P.optional(bg, -1));

		lua_render_ctx->queue->create_add<render_object_text>(x, y, text, fg, bg, hdbl, vdbl);
		return 0;
	}

	lua::functions text_fns(lua_func_misc, "gui", {
		{"text", internal_gui_text<false, false>},
		{"textH", internal_gui_text<true, false>},
		{"textV", internal_gui_text<false, true>},
		{"textHV", internal_gui_text<true, true>},
	});
}
