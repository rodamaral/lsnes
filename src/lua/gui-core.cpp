#include "lua/internal.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"

namespace
{
	class lua_gui_resolution : public lua_function
	{
	public:
		lua_gui_resolution() : lua_function(LS, "gui.resolution") {}
		int invoke(lua_state& L)
		{
			if(!lua_render_ctx)
				return 0;
			L.pushnumber(lua_render_ctx->width);
			L.pushnumber(lua_render_ctx->height);
			return 2;
		}
	} gui_resolution;

	template<uint32_t lua_render_context::*gap>
	class lua_gui_set_gap : public lua_function
	{
	public:
		lua_gui_set_gap(const std::string& name) : lua_function(LS, name) {}
		int invoke(lua_state& L)
		{
			if(!lua_render_ctx)
				return 0;
			uint32_t g = L.get_numeric_argument<uint32_t>(1, fname.c_str());
			if(g > 8192)
				return 0;	//Ignore ridiculous gap.
			lua_render_ctx->*gap = g;
			return 0;
		}
	};

	lua_gui_set_gap<&lua_render_context::left_gap> lg("gui.left_gap");
	lua_gui_set_gap<&lua_render_context::right_gap> rg("gui.right_gap");
	lua_gui_set_gap<&lua_render_context::top_gap> tg("gui.top_gap");
	lua_gui_set_gap<&lua_render_context::bottom_gap> bg("gui.bottom_gap");

	function_ptr_luafun gui_repaint(LS, "gui.repaint", [](lua_state& L, const std::string& fname) -> int {
		lua_requests_repaint = true;
		return 0;
	});

	function_ptr_luafun gui_sfupd(LS, "gui.subframe_update", [](lua_state& L, const std::string& fname) -> int {
		lua_requests_subframe_paint = L.get_bool(1, fname.c_str());
		return 0;
	});

	function_ptr_luafun gui_color(LS, "gui.color", [](lua_state& L, const std::string& fname) -> int {
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

	function_ptr_luafun gui_status(LS, "gui.status", [](lua_state& L, const std::string& fname) -> int {
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
		int16_t ohue = hue % (6 * S);
		hue = (hue + static_cast<uint32_t>(shift * S)) % (6 * S);
		uint32_t V[4];
		V[0] = m;
		V[1] = M;
		V[2] = m + hue % S;
		V[3] = M - hue % S;
		uint8_t flag = hsl2rgb_flags[hue / S];
		return (V[(flag >> 4) & 3] << 16) | (V[(flag >> 2) & 3] << 8) | (V[flag & 3]);
	}

	function_ptr_luafun gui_rainbow(LS, "gui.rainbow", [](lua_state& L, const std::string& fname) -> int {
		int64_t basecolor = 0x00FF0000;
		uint64_t step = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		int32_t steps = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<int64_t>(3, basecolor, fname.c_str());
		if(!steps) {
			L.pushstring("Expected nonzero steps for gui.rainbow");
			L.error();
		}
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
}
