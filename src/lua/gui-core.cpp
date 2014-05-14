#include "lua/internal.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"

void update_movie_state();

namespace
{
	std::map<std::string, std::u32string> lua_watch_vars;

	template<uint32_t lua::render_context::*gap, bool delta>
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

	int resolution(lua::state& L, lua::parameters& P)
	{
		if(!lua_render_ctx)
			return 0;
		L.pushnumber(lua_render_ctx->width);
		L.pushnumber(lua_render_ctx->height);
		return 2;
	}

	int repaint(lua::state& L, lua::parameters& P)
	{
		lua_requests_repaint = true;
		return 0;
	}

	int subframe_update(lua::state& L, lua::parameters& P)
	{
		P(lua_requests_subframe_paint);
		return 0;
	}

	int color(lua::state& L, lua::parameters& P)
	{
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
	}

	int status(lua::state& L, lua::parameters& P)
	{
		std::string name, value;

		P(name, value);

		if(value == "")
			lua_watch_vars.erase(name);
		else
			lua_watch_vars[name] = utf8::to32(value);
		update_movie_state();
		return 1;
	}

	int rainbow(lua::state& L, lua::parameters& P)
	{
		int64_t step, steps;
		framebuffer::color c;

		P(step, steps, P.optional(c, 0x00FF0000));

		auto basecolor = c.asnumber();
		if(!steps)
			throw std::runtime_error("Expected nonzero steps for gui.rainbow");
		basecolor = framebuffer::color_rotate_hue(basecolor, step, steps);
		L.pushnumber(basecolor);
		return 1;
	}

	int kill_frame(lua::state& L, lua::parameters& P)
	{
		if(lua_kill_frame)
			*lua_kill_frame = true;
		return 0;
	}

	int set_video_scale(lua::state& L, lua::parameters& P)
	{
		if(lua_hscl && lua_vscl) {
			uint32_t h, v;

			P(h, v);

			*lua_hscl = h;
			*lua_vscl = v;
		}
		return 0;
	}

	lua::functions guicore_fns(lua_func_misc, "gui", {
		{"left_gap", lua_gui_set_gap<&lua::render_context::left_gap, false>},
		{"right_gap", lua_gui_set_gap<&lua::render_context::right_gap, false>},
		{"top_gap", lua_gui_set_gap<&lua::render_context::top_gap, false>},
		{"bottom_gap", lua_gui_set_gap<&lua::render_context::bottom_gap, false>},
		{"delta_left_gap", lua_gui_set_gap<&lua::render_context::left_gap, true>},
		{"delta_right_gap", lua_gui_set_gap<&lua::render_context::right_gap, true>},
		{"delta_top_gap", lua_gui_set_gap<&lua::render_context::top_gap, true>},
		{"delta_bottom_gap", lua_gui_set_gap<&lua::render_context::bottom_gap, true>},
		{"resolution", resolution},
		{"repaint", repaint},
		{"subframe_update", subframe_update},
		{"color", color},
		{"status", status},
		{"rainbow", rainbow},
		{"kill_frame", kill_frame},
		{"set_video_scale", set_video_scale},
	});
}

const std::map<std::string, std::u32string>& get_lua_watch_vars()
{
	return lua_watch_vars;
}