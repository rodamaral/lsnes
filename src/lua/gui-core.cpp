#include "lua/internal.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"

namespace
{
	lua::fnptr2 gui_resolution(lua_func_misc, "gui.resolution", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_render_ctx)
			return 0;
		L.pushnumber(lua_render_ctx->width);
		L.pushnumber(lua_render_ctx->height);
		return 2;
	});

	template<uint32_t lua_render_context::*gap, bool delta>
	int lua_gui_set_gap(lua::state& L, lua::parameters& P)
	{
		int32_t g;
		if(!lua_render_ctx)
			return 0;

		P(g);

		if(lua_render_ctx->*gap == std::numeric_limits<uint32_t>::max())
			lua_render_ctx->*gap = 0;  //Handle default gap of render queue.
		if(delta) g += lua_render_ctx->*gap;
		if(g < 0 || g > 8192)
			return 0;	//Ignore ridiculous gap.
		uint32_t old = lua_render_ctx->*gap;
		lua_render_ctx->*gap = g;
		L.pushnumber(old);
		return 1;
	}

	lua::fnptr2 lg(lua_func_misc, "gui.left_gap", lua_gui_set_gap<&lua_render_context::left_gap, false>);
	lua::fnptr2 rg(lua_func_misc, "gui.right_gap", lua_gui_set_gap<&lua_render_context::right_gap, false>);
	lua::fnptr2 tg(lua_func_misc, "gui.top_gap", lua_gui_set_gap<&lua_render_context::top_gap, false>);
	lua::fnptr2 bg(lua_func_misc, "gui.bottom_gap", lua_gui_set_gap<&lua_render_context::bottom_gap, false>);
	lua::fnptr2 dlg(lua_func_misc, "gui.delta_left_gap", lua_gui_set_gap<&lua_render_context::left_gap, true>);
	lua::fnptr2 drg(lua_func_misc, "gui.delta_right_gap", lua_gui_set_gap<&lua_render_context::right_gap, true>);
	lua::fnptr2 dtg(lua_func_misc, "gui.delta_top_gap", lua_gui_set_gap<&lua_render_context::top_gap, true>);
	lua::fnptr2 dbg(lua_func_misc, "gui.delta_bottom_gap", lua_gui_set_gap<&lua_render_context::bottom_gap,
		true>);

	lua::fnptr2 gui_repaint(lua_func_misc, "gui.repaint", [](lua::state& L, lua::parameters& P) -> int {
		lua_requests_repaint = true;
		return 0;
	});

	lua::fnptr2 gui_sfupd(lua_func_misc, "gui.subframe_update", [](lua::state& L, lua::parameters& P) -> int {
		P(lua_requests_subframe_paint);
		return 0;
	});

	lua::fnptr2 gui_color(lua_func_misc, "gui.color", [](lua::state& L, lua::parameters& P) -> int {
		int64_t r, g, b, a;

		if(P.is_string()) {
			framebuffer::color c(P.arg<std::string>());
			L.pushnumber(c.asnumber());
			return 1;
		}

		P(r, g, b, P.optional(a, 256));

		if(a > 0)
			L.pushnumber(((256 - a) << 24) | (r << 16) | (g << 8) | b);
		else
			L.pushnumber(-1);
		return 1;
	});

	lua::fnptr2 gui_status(lua_func_misc, "gui.status", [](lua::state& L, lua::parameters& P) -> int {
		std::string name, value;

		P(name, value);

		auto& w = platform::get_emustatus();
		if(value == "")
			w.erase("L[" + name + "]");
		else
			w.set("L[" + name + "]", value);
		return 1;
	});

	lua::fnptr2 gui_rainbow(lua_func_misc, "gui.rainbow", [](lua::state& L, lua::parameters& P) -> int {
		int64_t step, steps;
		framebuffer::color c;

		P(step, steps, P.optional(c, 0x00FF0000));

		auto basecolor = c.asnumber();
		if(!steps)
			throw std::runtime_error("Expected nonzero steps for gui.rainbow");
		basecolor = framebuffer::color_rotate_hue(basecolor, step, steps);
		L.pushnumber(basecolor);
		return 1;
	});

	lua::fnptr2 gui_killframe(lua_func_misc, "gui.kill_frame", [](lua::state& L, lua::parameters& P) -> int {
		if(lua_kill_frame)
			*lua_kill_frame = true;
	});

	lua::fnptr2 gui_setscale(lua_func_misc, "gui.set_video_scale", [](lua::state& L, lua::parameters& P) -> int {
		if(lua_hscl && lua_vscl) {
			uint32_t h, v;

			P(h, v);

			*lua_hscl = h;
			*lua_vscl = v;
		}
	});
}
