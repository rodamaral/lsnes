#include "lua/internal.hpp"
#include "fonts/wrapper.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/customfont.hpp"
#include "library/utf8.hpp"
#include <algorithm>


namespace
{
	struct lua_customfont
	{
	public:
		lua_customfont(lua_state& L, const std::string& filename);
		lua_customfont(lua_state& L);
		~lua_customfont() throw();
		int draw(lua_state& L, const std::string& fname);
		const custom_font& get_font() { return font; }
		std::string print()
		{
			return orig_filename;
		}
	private:
		void init(lua_state& L);
		std::string orig_filename;
		custom_font font;
	};
}

DECLARE_LUACLASS(lua_customfont, "CUSTOMFONT");

namespace
{
	struct render_object_text_cf : public framebuffer::object
	{
		render_object_text_cf(int32_t _x, int32_t _y, const std::string& _text, framebuffer::color _fg,
			framebuffer::color _bg, framebuffer::color _hl, lua_obj_pin<lua_customfont> _font) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hl(_hl), font(_font) {}
		~render_object_text_cf() throw()
		{
		}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			hl.set_palette(scr);
			const custom_font& fdata = font->get_font();
			std::u32string _text = to_u32string(text);
			int32_t orig_x = x;
			int32_t drawx = x;
			int32_t drawy = y;
			if(hl) {
				//Adjust for halo.
				drawx++;
				orig_x++;
				drawy++;
			}
			for(size_t i = 0; i < _text.size();) {
				uint32_t cp = _text[i];
				std::u32string k = fdata.best_ligature_match(_text, i);
				const font_glyph_data& glyph = fdata.lookup_glyph(k);
				if(k.length())
					i += k.length();
				else
					i++;
				if(cp == 9) {
					drawx = (drawx + 64) >> 6 << 6;
				} else if(cp == 10) {
					drawx = orig_x;
					drawy += fdata.get_rowadvance();
				} else {
					glyph.render(scr, drawx, drawy, fg, bg, hl);
					drawx += glyph.width;
				}
			}
		}
		bool kill_request(void* obj) throw()
		{
			return kill_request_ifeq(font.object(), obj);
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
		framebuffer::color hl;
		lua_obj_pin<lua_customfont> font;
	};

	void lua_customfont::init(lua_state& L)
	{
		objclass<lua_customfont>().bind_multi(L, {
			{"__call", &lua_customfont::draw},
		});
	}

	lua_customfont::lua_customfont(lua_state& L, const std::string& filename)
		: font(filename)
	{
		orig_filename = filename;
		init(L);
	}

	lua_customfont::lua_customfont(lua_state& L)
		: font(main_font)
	{
		orig_filename = "<builtin>";
		init(L);
	}

	lua_customfont::~lua_customfont() throw()
	{
		render_kill_request(this);
	}

	int lua_customfont::draw(lua_state& L, const std::string& fname)
	{
		if(!lua_render_ctx)
			return 0;
		int64_t fgc = 0xFFFFFFU;
		int64_t bgc = -1;
		int64_t hlc = -1;
		int32_t _x = L.get_numeric_argument<int32_t>(2, fname.c_str());
		int32_t _y = L.get_numeric_argument<int32_t>(3, fname.c_str());
		L.get_numeric_argument<int64_t>(5, fgc, fname.c_str());
		L.get_numeric_argument<int64_t>(6, bgc, fname.c_str());
		L.get_numeric_argument<int64_t>(7, hlc, fname.c_str());
		std::string text = L.get_string(4, fname.c_str());
		auto f = lua_class<lua_customfont>::pin(L, 1, fname.c_str());
		framebuffer::color fg(fgc);
		framebuffer::color bg(bgc);
		framebuffer::color hl(hlc);
		lua_render_ctx->queue->create_add<render_object_text_cf>(_x, _y, text, fg, bg, hl, f);
		return 0;
	}

	function_ptr_luafun gui_text_cf(lua_func_misc, "gui.loadfont", [](lua_state& L, const std::string& fname)
		-> int {
		if(L.type(1) == LUA_TNONE || L.type(1) == LUA_TNIL) {
			lua_class<lua_customfont>::create(L);
			return 1;
		}
		std::string filename = L.get_string(1, fname.c_str());
		lua_class<lua_customfont>::create(L, filename);
		return 1;
	});
}
