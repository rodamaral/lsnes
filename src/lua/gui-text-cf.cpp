#include "core/instance.hpp"
#include "lua/internal.hpp"
#include "lua/bitmap.hpp"
#include "fonts/wrapper.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "library/framebuffer-font2.hpp"
#include "library/utf8.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/zip.hpp"
#include <algorithm>


namespace
{
	struct lua_customfont
	{
	public:
		struct empty_font_tag {};
		lua_customfont(lua::state& L, const std::string& filename, const std::string& filename2);
		lua_customfont(lua::state& L);
		lua_customfont(lua::state& L, empty_font_tag tag);
		static size_t overcommit() { return 0; }
		static size_t overcommit(const std::string& filename, const std::string& filename2) { return 0; }
		static size_t overcommit(empty_font_tag tag) { return 0; }
		~lua_customfont() throw();
		static int create(lua::state& L, lua::parameters& P);
		static int load(lua::state& L, lua::parameters& P);
		int draw(lua::state& L, lua::parameters& P);
		int edit(lua::state& L, lua::parameters& P);
		const framebuffer::font2& get_font() { return font; }
		std::string print()
		{
			return orig_filename;
		}
	private:
		std::string orig_filename;
		framebuffer::font2 font;
	};

	lua::_class<lua_customfont> LUA_class_customfont(lua_class_gui, "CUSTOMFONT", {
		{"new", lua_customfont::create},
		{"load", lua_customfont::load},
	}, {
		{"__call", &lua_customfont::draw},
		{"edit", &lua_customfont::edit},
	}, &lua_customfont::print);

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

	lua_customfont::lua_customfont(lua::state& L, const std::string& filename, const std::string& filename2)
		: orig_filename(zip::resolverel(filename, filename2)), font(orig_filename)
	{
	}

	lua_customfont::lua_customfont(lua::state& L)
		: font(main_font)
	{
		orig_filename = "<builtin>";
	}

	lua_customfont::~lua_customfont() throw()
	{
		CORE().fbuf->render_kill_request(this);
	}

	int lua_customfont::draw(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		int32_t _x, _y;
		framebuffer::color fg, bg, hl;
		std::string text;
		lua::objpin<lua_customfont> f;

		if(!core.lua2->render_ctx)
			return 0;

		P(f, _x, _y, text, P.optional(fg, 0xFFFFFFU), P.optional(bg, -1), P.optional(hl, -1));

		core.lua2->render_ctx->queue->create_add<render_object_text_cf>(_x, _y, text, fg, bg, hl, f);
		return 0;
	}

	lua_customfont::lua_customfont(lua::state& L, lua_customfont::empty_font_tag tag)
	{
		orig_filename = "<empty>";
	}

	int lua_customfont::edit(lua::state& L, lua::parameters& P)
	{
		std::string text;
		lua_bitmap* _glyph;

		P(P.skipped(), text, _glyph);

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
		return 0;
	}

	int lua_customfont::create(lua::state& L, lua::parameters& P)
	{
		lua::_class<lua_customfont>::create(L, lua_customfont::empty_font_tag());
		return 1;
	};

	int lua_customfont::load(lua::state& L, lua::parameters& P)
	{
		std::string filename, filename2;
		if(P.is_novalue()) {
			lua::_class<lua_customfont>::create(L);
			return 1;
		}

		P(filename, P.optional(filename2, ""));

		lua::_class<lua_customfont>::create(L, filename, filename2);
		return 1;
	};
}
