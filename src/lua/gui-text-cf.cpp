#include "lua/internal.hpp"
#include "fonts/wrapper.hpp"
#include "core/framebuffer.hpp"
#include "library/framebuffer.hpp"
#include "library/customfont.hpp"
#include "library/utf8.hpp"


namespace
{
	struct lua_customfont
	{
	public:
		lua_customfont(lua_State* LS, const std::string& filename);
		~lua_customfont() throw();
		int draw(lua_State* LS);
		const custom_font& get_font() { return font; }
		std::string print()
		{
			return "";
		}
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
			premultiplied_color _bg, premultiplied_color _hl, lua_obj_pin<lua_customfont>* _font) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hl(_hl), font(_font) {}
		~render_object_text_cf() throw()
		{
			delete font;
		}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			hl.set_palette(scr);
			const custom_font& fdata = font->object()->get_font();
			std::vector<uint32_t> _text = decode_utf8(text);
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
					glyph.render(scr, drawx, drawy, fg, bg, hl);
					drawx += glyph.width;
				}
			}
			
		}
		bool kill_request(void* obj) throw()
		{
			return kill_request_ifeq(unbox_any_pin(font), obj);
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
	private:
		int32_t x;
		int32_t y;
		premultiplied_color fg;
		premultiplied_color bg;
		premultiplied_color hl;
		std::string text;
		lua_obj_pin<lua_customfont>* font;
	};

	lua_customfont::lua_customfont(lua_State* LS, const std::string& filename)
		: font(filename) 
	{
		static char done_key;
		if(lua_do_once(LS, &done_key)) {
			objclass<lua_customfont>().bind(LS, "__call", &lua_customfont::draw);
		}
	}
	
	lua_customfont::~lua_customfont() throw()
	{
		render_kill_request(this);
	}
	
	int lua_customfont::draw(lua_State* LS)
	{
		std::string fname = "CUSTOMFONT::__call";
		if(!lua_render_ctx)
			return 0;
		int64_t fgc = 0xFFFFFFU;
		int64_t bgc = -1;
		int64_t hlc = -1;
		int32_t _x = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		int32_t _y = get_numeric_argument<int32_t>(LS, 3, fname.c_str());
		get_numeric_argument<int64_t>(LS, 5, fgc, fname.c_str());
		get_numeric_argument<int64_t>(LS, 6, bgc, fname.c_str());
		get_numeric_argument<int64_t>(LS, 7, hlc, fname.c_str());
		std::string text = get_string_argument(LS, 4, fname.c_str());
		auto f = lua_class<lua_customfont>::pin(LS, 1, fname.c_str());
		premultiplied_color fg(fgc);
		premultiplied_color bg(bgc);
		premultiplied_color hl(hlc);
		lua_render_ctx->queue->create_add<render_object_text_cf>(_x, _y, text, fg, bg, hl, f);
		return 0;
	}

	function_ptr_luafun gui_text_cf("gui.loadfont", [](lua_State* LS, const std::string& fname) -> int {
		std::string filename = get_string_argument(LS, 1, fname.c_str());
		lua_customfont* b = lua_class<lua_customfont>::create(LS, LS, filename);
		return 1;
	
	});
}
