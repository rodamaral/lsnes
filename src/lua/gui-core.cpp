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

	lua::fnptr gui_repaint(lua_func_misc, "gui.repaint", [](lua::state& L, const std::string& fname)
		-> int {
		lua_requests_repaint = true;
		return 0;
	});

	lua::fnptr gui_sfupd(lua_func_misc, "gui.subframe_update", [](lua::state& L, const std::string& fname)
		-> int {
		lua_requests_subframe_paint = L.get_bool(1, fname.c_str());
		return 0;
	});

	lua::fnptr gui_color(lua_func_misc, "gui.color", [](lua::state& L, const std::string& fname)
		-> int {
		int64_t a = 256;
		int64_t r = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		int64_t g = L.get_numeric_argument<uint32_t>(2, fname.c_str());
		int64_t b = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		L.get_numeric_argument<int64_t>(4, a, fname.c_str());
		if(a > 0)
			L.pushnumber(((256 - a) << 24) | (r << 16) | (g << 8) | b);
		else
			L.pushnumber(-1);
		return 1;
	});

	lua::fnptr gui_status(lua_func_misc, "gui.status", [](lua::state& L, const std::string& fname)
		-> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string value = L.get_string(2, fname.c_str());
		auto& w = platform::get_emustatus();
		if(value == "")
			w.erase("L[" + name + "]");
		else
			w.set("L[" + name + "]", value);
		return 1;
	});


	//0: m
	//1: M
	//2: m + phue
	//3: M - phue
	uint8_t hsl2rgb_flags[] = {24, 52, 6, 13, 33, 19};

	uint32_t shifthue(uint32_t color, double shift)
	{
		int16_t R = (color >> 16) & 0xFF;
		int16_t G = (color >> 8) & 0xFF;
		int16_t B = color & 0xFF;
		int16_t m = min(R, min(G, B));
		int16_t M = max(R, max(G, B));
		int16_t S = M - m;
		if(!S)
			return color;	//Grey.
		int16_t hue;
		if(R == M)
			hue = G - B + 6 * S;
		else if(G == M)
			hue = B - R + 2 * S;
		else
			hue = R - G + 4 * S;
		hue = (hue + static_cast<uint32_t>(shift * S)) % (6 * S);
		uint32_t V[4];
		V[0] = m;
		V[1] = M;
		V[2] = m + hue % S;
		V[3] = M - hue % S;
		uint8_t flag = hsl2rgb_flags[hue / S];
		return (V[(flag >> 4) & 3] << 16) | (V[(flag >> 2) & 3] << 8) | (V[flag & 3]);
	}

	lua::fnptr gui_rainbow(lua_func_misc, "gui.rainbow", [](lua::state& L, const std::string& fname)
		-> int {
		int64_t basecolor = 0x00FF0000;
		uint64_t step = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		int32_t steps = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<int64_t>(3, basecolor, fname.c_str());
		if(!steps)
			throw std::runtime_error("Expected nonzero steps for gui.rainbow");
		if(basecolor < 0) {
			//Special: Any rotation of transparent is transparent.
			L.pushnumber(-1);
			return 1;
		}
		uint32_t asteps = std::abs(steps);
		if(steps < 0)
			step = asteps - step % asteps;	//Reverse order.
		double hueshift = 6.0 * (step % asteps) / asteps;
		basecolor = shifthue(basecolor & 0xFFFFFF, hueshift) | (basecolor & 0xFF000000);
		L.pushnumber(basecolor);
		return 1;
	});

	lua::fnptr gui_killframe(lua_func_misc, "gui.kill_frame", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua_kill_frame)
			*lua_kill_frame = true;
	});
}
