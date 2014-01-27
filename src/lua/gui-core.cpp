#include "lua/internal.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"

namespace
{
	class lua_gui_resolution : public lua::function
	{
	public:
		lua_gui_resolution() : lua::function(lua_func_misc, "gui.resolution") {}
		int invoke(lua::state& L)
		{
			if(!lua_render_ctx)
				return 0;
			L.pushnumber(lua_render_ctx->width);
			L.pushnumber(lua_render_ctx->height);
			return 2;
		}
	} gui_resolution;

	template<uint32_t lua_render_context::*gap, bool delta>
	class lua_gui_set_gap : public lua::function
	{
	public:
		lua_gui_set_gap(const std::string& name) : lua::function(lua_func_misc, name) {}
		int invoke(lua::state& L)
		{
			if(!lua_render_ctx)
				return 0;
			int32_t g = L.get_numeric_argument<int32_t>(1, fname.c_str());
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
	};

	lua_gui_set_gap<&lua_render_context::left_gap, false> lg("gui.left_gap");
	lua_gui_set_gap<&lua_render_context::right_gap, false> rg("gui.right_gap");
	lua_gui_set_gap<&lua_render_context::top_gap, false> tg("gui.top_gap");
	lua_gui_set_gap<&lua_render_context::bottom_gap, false> bg("gui.bottom_gap");
	lua_gui_set_gap<&lua_render_context::left_gap, true> dlg("gui.delta_left_gap");
	lua_gui_set_gap<&lua_render_context::right_gap, true> drg("gui.delta_right_gap");
	lua_gui_set_gap<&lua_render_context::top_gap, true> dtg("gui.delta_top_gap");
	lua_gui_set_gap<&lua_render_context::bottom_gap, true> dbg("gui.delta_bottom_gap");

	lua::fnptr2 gui_repaint(lua_func_misc, "gui.repaint", [](lua::state& L, lua::parameters& P) -> int {
		lua_requests_repaint = true;
		return 0;
	});

	lua::fnptr2 gui_sfupd(lua_func_misc, "gui.subframe_update", [](lua::state& L, lua::parameters& P) -> int {
		lua_requests_subframe_paint = P.arg<bool>();
		return 0;
	});

	lua::fnptr2 gui_color(lua_func_misc, "gui.color", [](lua::state& L, lua::parameters& P) -> int {
		if(P.is_string()) {
			framebuffer::color c(P.arg<std::string>());
			L.pushnumber(c.asnumber());
			return 1;
		}
		int64_t r = P.arg<int64_t>();
		int64_t g = P.arg<int64_t>();
		int64_t b = P.arg<int64_t>();
		int64_t a = P.arg_opt<int64_t>(256);
		if(a > 0)
			L.pushnumber(((256 - a) << 24) | (r << 16) | (g << 8) | b);
		else
			L.pushnumber(-1);
		return 1;
	});

	lua::fnptr2 gui_status(lua_func_misc, "gui.status", [](lua::state& L, lua::parameters& P) -> int {
		auto name = P.arg<std::string>();
		auto value = P.arg<std::string>();
		auto& w = platform::get_emustatus();
		if(value == "")
			w.erase("L[" + name + "]");
		else
			w.set("L[" + name + "]", value);
		return 1;
	});

	lua::fnptr2 gui_rainbow(lua_func_misc, "gui.rainbow", [](lua::state& L, lua::parameters& P) -> int {
		int64_t step = P.arg<int64_t>();
		int64_t steps = P.arg<int64_t>();
		auto basecolor = P.arg_opt<framebuffer::color>(0x00FF0000).asnumber();
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
			auto h = P.arg<uint32_t>();
			auto v = P.arg<uint32_t>();
			*lua_hscl = h;
			*lua_vscl = v;
		}
	});
}
