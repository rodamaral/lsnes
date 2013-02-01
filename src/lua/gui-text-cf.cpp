#include "lua/internal.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/customfont.hpp"
#include "library/utf8.hpp"


namespace
{
	struct lua_customfont
	{
	public:
		lua_customfont(lua_state* L, const std::string& filename);
		~lua_customfont() throw();
		int draw(lua_state& L);
		const custom_font& get_font() { return font; }
	private:
		custom_font font;
	};
}

DECLARE_LUACLASS(lua_customfont, "CUSTOMFONT");

namespace
{
	std::vector<uint32_t> decode_utf8(const std::string& t)
	{
		std::vector<uint32_t> x;
		size_t ts = t.length();
		uint16_t s = utf8_initial_state;
		for(size_t i = 0; i <= ts; i++) {
			int ch = (i < ts) ? (int)(unsigned char)t[i] : -1;
			int32_t cp = utf8_parse_byte(ch, s);
			if(cp >= 0)
				x.push_back(cp);
		}
		return x;
	}
	
	struct render_object_text_cf : public render_object
	{
		render_object_text_cf(int32_t _x, int32_t _y, const std::string& _text, premultiplied_color _fg,
			premultiplied_color _bg, lua_obj_pin<lua_customfont>* _font) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), font(_font) {}
		~render_object_text_cf() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			const custom_font& fdata = font->object()->get_font();
			std::vector<uint32_t> _text = decode_utf8(text);
			int32_t orig_x = x;
			int32_t drawx = x;
			int32_t drawy = y;
			for(size_t i = 0; i < _text.size();) {
				uint32_t cp = _text[i];
				ligature_key k = fdata.best_ligature_match(_text, i);
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
					glyph.render(scr, drawx, drawy, fg, bg);
					drawx += glyph.width;
				}
			}
			
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
		void clone(render_queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		premultiplied_color fg;
		premultiplied_color bg;
		std::string text;
		lua_obj_pin<lua_customfont>* font;
	};

	lua_customfont::lua_customfont(lua_state* L, const std::string& filename)
		: font(filename) 
	{
		static char done_key;
		if(L->do_once(&done_key)) {
			objclass<lua_customfont>().bind(*L, "__call", &lua_customfont::draw);
		}
	}
	
	lua_customfont::~lua_customfont() throw() {}
	
	int lua_customfont::draw(lua_state& L)
	{
		std::string fname = "CUSTOMFONT::__call";
		if(!lua_render_ctx)
			return 0;
		int64_t fgc = 0xFFFFFFU;
		int64_t bgc = -1;
		int32_t _x = L.get_numeric_argument<int32_t>(2, fname.c_str());
		int32_t _y = L.get_numeric_argument<int32_t>(3, fname.c_str());
		L.get_numeric_argument<int64_t>(5, fgc, fname.c_str());
		L.get_numeric_argument<int64_t>(6, bgc, fname.c_str());
		std::string text = L.get_string(4, fname.c_str());
		auto f = lua_class<lua_customfont>::pin(L, 1, fname.c_str());
		premultiplied_color fg(fgc);
		premultiplied_color bg(bgc);
		lua_render_ctx->queue->create_add<render_object_text_cf>(_x, _y, text, fg, bg, f);
		return 0;
	}

	function_ptr_luafun gui_text_cf(LS, "gui.loadfont", [](lua_state& L, const std::string& fname) -> int {
		std::string filename = L.get_string(1, fname.c_str());
		try {
			lua_customfont* b = lua_class<lua_customfont>::create(L, &L, filename);
			return 1;
		} catch(std::exception& e) {
			L.pushstring(e.what());
			L.error();
			return 0;
		}
	});
}

