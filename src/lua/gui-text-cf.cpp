#include "lua/internal.hpp"
#include "lua/bitmap.hpp"
#include "fonts/wrapper.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/framebuffer-font2.hpp"
#include "library/utf8.hpp"
#include "library/lua-framebuffer.hpp"
#include <algorithm>


namespace
{
	struct lua_customfont
	{
	public:
		struct empty_font_tag {};
		lua_customfont(lua::state& L, const std::string& filename);
		lua_customfont(lua::state& L);
		lua_customfont(lua::state& L, empty_font_tag tag);
		~lua_customfont() throw();
		int draw(lua::state& L, const std::string& fname);
		int edit(lua::state& L, const std::string& fname);
		const framebuffer::font2& get_font() { return font; }
		std::string print()
		{
			return orig_filename;
		}
	private:
		void init(lua::state& L);
		std::string orig_filename;
		framebuffer::font2 font;
	};

	lua::_class<lua_customfont> class_customfont("CUSTOMFONT");

	struct render_object_text_cf : public framebuffer::object
	{
		render_object_text_cf(int32_t _x, int32_t _y, const std::string& _text, framebuffer::color _fg,
			framebuffer::color _bg, framebuffer::color _hl, lua::objpin<lua_customfont> _font) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hl(_hl), font(_font) {}
		~render_object_text_cf() throw()
		{
		}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			hl.set_palette(scr);
			const framebuffer::font2& fdata = font->get_font();
			std::u32string _text = utf8::to32(text);
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
				const framebuffer::font2::glyph& glyph = fdata.lookup_glyph(k);
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
		lua::objpin<lua_customfont> font;
	};

	void lua_customfont::init(lua::state& L)
	{
		lua::objclass<lua_customfont>().bind_multi(L, {
			{"__call", &lua_customfont::draw},
			{"edit", &lua_customfont::edit},
		});
	}

	lua_customfont::lua_customfont(lua::state& L, const std::string& filename)
		: font(filename)
	{
		orig_filename = filename;
		init(L);
	}

	lua_customfont::lua_customfont(lua::state& L)
		: font(main_font)
	{
		orig_filename = "<builtin>";
		init(L);
	}

	lua_customfont::~lua_customfont() throw()
	{
		render_kill_request(this);
	}

	int lua_customfont::draw(lua::state& L, const std::string& fname)
	{
		if(!lua_render_ctx)
			return 0;
		int32_t _x = L.get_numeric_argument<int32_t>(2, fname.c_str());
		int32_t _y = L.get_numeric_argument<int32_t>(3, fname.c_str());
		auto fg = lua_get_fb_color(L, 5, fname, 0xFFFFFFU);
		auto bg = lua_get_fb_color(L, 6, fname, -1);
		auto hl = lua_get_fb_color(L, 7, fname, -1);
		std::string text = L.get_string(4, fname.c_str());
		auto f = lua::_class<lua_customfont>::pin(L, 1, fname.c_str());
		lua_render_ctx->queue->create_add<render_object_text_cf>(_x, _y, text, fg, bg, hl, f);
		return 0;
	}

	lua_customfont::lua_customfont(lua::state& L, lua_customfont::empty_font_tag tag)
	{
		orig_filename = "<empty>";
		init(L);
	}

	int lua_customfont::edit(lua::state& L, const std::string& fname)
	{
		std::string text = L.get_string(2, fname.c_str());
		lua_bitmap* _glyph = lua::_class<lua_bitmap>::get(L, 3, fname.c_str());
		framebuffer::font2::glyph glyph;
		glyph.width = _glyph->width;
		glyph.height = _glyph->height;
		glyph.stride = (glyph.width + 31) / 32;
		glyph.fglyph.resize(glyph.stride * glyph.height);
		memset(&glyph.fglyph[0], 0, sizeof(uint32_t) * glyph.fglyph.size());
		for(size_t y = 0; y < glyph.height; y++) {
			size_t bpos = y * glyph.stride * 32;
			for(size_t x = 0; x < glyph.width; x++) {
				size_t e = (bpos + x) / 32;
				size_t b = 31 - (bpos + x) % 32;
				if(_glyph->pixels[y * _glyph->width + x])
					glyph.fglyph[e] |= (1UL << b);
			}
		}
		font.add(utf8::to32(text), glyph);
	}

	lua::fnptr gui_text_cf_e(lua_func_misc, "gui.font_new", [](lua::state& L, const std::string& fname)
		-> int {
		lua::_class<lua_customfont>::create(L, lua_customfont::empty_font_tag());
		return 1;
	});

	lua::fnptr gui_text_cf(lua_func_misc, "gui.loadfont", [](lua::state& L, const std::string& fname)
		-> int {
		if(L.type(1) == LUA_TNONE || L.type(1) == LUA_TNIL) {
			lua::_class<lua_customfont>::create(L);
			return 1;
		}
		std::string filename = L.get_string(1, fname.c_str());
		lua::_class<lua_customfont>::create(L, filename);
		return 1;
	});
}
