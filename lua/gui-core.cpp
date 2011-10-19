#include "lua-int.hpp"

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

	template<uint32_t lua_render_context::*gap>
	class lua_gui_set_gap : public lua_function
	{
	public:
		lua_gui_set_gap(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			if(!lua_render_ctx)
				return 0;
			uint32_t g = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
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

	function_ptr_luafun gui_repaint("gui.repaint", [](lua_State* LS, const std::string& fname) -> int {
		lua_requests_repaint = true;
		return 0;
	});

	function_ptr_luafun gui_sfupd("gui.subframe_update", [](lua_State* LS, const std::string& fname) -> int {
		lua_requests_subframe_paint = get_boolean_argument(LS, 1, fname.c_str());
		return 0;
	});

	function_ptr_luafun gui_color("gui.color", [](lua_State* LS, const std::string& fname) -> int {
		uint32_t r = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		uint32_t g = get_numeric_argument<uint32_t>(LS, 2, fname.c_str());
		uint32_t b = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		lua_pushnumber(LS, ((r << 7) & 0x7C00) | ((g << 2) & 0x3E0) | ((b >> 3) & 0x1F));
		return 1;
	});
}
