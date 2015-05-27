#include "core/instance.hpp"
#include "lua/internal.hpp"
#include "lua/halo.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

namespace
{
	struct render_object_text : public framebuffer::object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, framebuffer::color _fg,
			framebuffer::color _bg, framebuffer::color _hl, bool _hdbl = false, bool _vdbl = false)
			throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hl(_hl), hdbl(_hdbl), vdbl(_vdbl) {}
		~render_object_text() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			auto size = main_font.get_metrics(text, x, hdbl, vdbl);
			//Enlarge size by 2 in each dimension, in order to accomodiate halo, if any.
			//Round up width to multiple of 32.
			size.first = (size.first + 33) >> 5 << 5;
			size.second += 2;
			//The -1 is to accomodiate halo.
			size_t allocsize = size.first * size.second + 32;

			if(allocsize > 32768) {
				std::vector<uint8_t> memory;
				memory.resize(allocsize);
				op_with(scr, &memory[0], size);
			} else {
				uint8_t memory[allocsize];
				op_with(scr, memory, size);
			}
		}
		template<bool X> void op_with(struct framebuffer::fb<X>& scr, unsigned char* mem,
			std::pair<size_t, size_t> size) throw()
		{
			uint32_t rx = x + (int32_t)scr.get_origin_x() - 1;
			uint32_t ry = y + (int32_t)scr.get_origin_y() - 1;
			mem += (32 - ((size_t)mem & 31)) & 31;	//Align.
			memset(mem, 0, size.first * size.second);
			main_font.render(mem + size.first + 1, size.first, text, x, hdbl, vdbl);
			halo_blit(scr, mem, size.first, size.second, rx, ry, bg, fg, hl);
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
		bool hdbl;
		bool vdbl;
	};

	template<bool hdbl, bool vdbl>
	int internal_gui_text(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		int32_t x, y;
		std::string text;
		framebuffer::color fg, bg, hl;

		if(!core.lua2->render_ctx) return 0;

		P(x, y, text, P.optional(fg, 0xFFFFFFU), P.optional(bg, -1), P.optional(hl, -1));

		core.lua2->render_ctx->queue->create_add<render_object_text>(x, y, text, fg, bg, hl, hdbl, vdbl);
		return 0;
	}

	lua::functions LUA_text_fns(lua_func_misc, "gui", {
		{"text", internal_gui_text<false, false>},
		{"textH", internal_gui_text<true, false>},
		{"textV", internal_gui_text<false, true>},
		{"textHV", internal_gui_text<true, true>},
	});
}
