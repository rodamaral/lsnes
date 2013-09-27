#include "lua/internal.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"

namespace
{
	class lua_gui_resolution : public lua_function
	{
	public:
		lua_gui_resolution() : lua_function("gui.resolution") {}
		int invoke(lua_State* LS)
		{
			if(!lua_render_ctx)
				return 0;
			lua_pushnumber(LS, lua_render_ctx->width);
			lua_pushnumber(LS, lua_render_ctx->height);
			return 2;
		}
	} gui_resolution;

	template<uint32_t lua_render_context::*gap, bool delta>
	class lua_gui_set_gap : public lua_function
	{
	public:
		lua_gui_set_gap(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			if(!lua_render_ctx)
				return 0;
			int32_t _g = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
			if(delta) _g += lua_render_ctx->*gap;
			if(_g < 0 || _g > 8192)
				return 0;	//Ignore ridiculous gap.
			uint32_t old = lua_render_ctx->*gap;
			lua_render_ctx->*gap = _g;
			lua_pushnumber(LS, old);
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

	function_ptr_luafun gui_repaint("gui.repaint", [](lua_State* LS, const std::string& fname) -> int {
		lua_requests_repaint = true;
		return 0;
	});

	function_ptr_luafun gui_sfupd("gui.subframe_update", [](lua_State* LS, const std::string& fname) -> int {
		lua_requests_subframe_paint = get_boolean_argument(LS, 1, fname.c_str());
		return 0;
	});

	function_ptr_luafun gui_color("gui.color", [](lua_State* LS, const std::string& fname) -> int {
		int64_t a = 256;
		int64_t r = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		int64_t g = get_numeric_argument<uint32_t>(LS, 2, fname.c_str());
		int64_t b = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		get_numeric_argument<int64_t>(LS, 4, a, fname.c_str());
		if(a > 0)
			lua_pushnumber(LS, ((256 - a) << 24) | (r << 16) | (g << 8) | b);
		else
			lua_pushnumber(LS, -1);
		return 1;
	});

	function_ptr_luafun gui_status("gui.status", [](lua_State* LS, const std::string& fname) -> int {
		std::string name = get_string_argument(LS, 1, fname.c_str());
		std::string value = get_string_argument(LS, 2, fname.c_str());
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

	function_ptr_luafun gui_rainbow("gui.rainbow", [](lua_State* LS, const std::string& fname) -> int {
		int64_t basecolor = 0x00FF0000;
		uint64_t step = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		int32_t steps = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		get_numeric_argument<int64_t>(LS, 3, basecolor, fname.c_str());
		if(!steps)
			throw std::runtime_error("Expected nonzero steps for gui.rainbow");
		if(basecolor < 0) {
			//Special: Any rotation of transparent is transparent.
			lua_pushnumber(LS, -1);
			return 1;
		}
		uint32_t asteps = std::abs(steps);
		if(steps < 0)
			step = asteps - step % asteps;	//Reverse order.
		double hueshift = 6.0 * (step % asteps) / asteps;
		basecolor = shifthue(basecolor & 0xFFFFFF, hueshift) | (basecolor & 0xFF000000);
		lua_pushnumber(LS, basecolor);
		return 1;
	});
}
