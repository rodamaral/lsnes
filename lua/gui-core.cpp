#include "lua-int.hpp"

namespace
{
	class lua_gui_resolution : public lua_function
	{
	public:
		lua_gui_resolution() : lua_function("gui.resolution") {}
		int invoke(lua_State* LS, window* win)
		{
			if(!lua_render_ctx)
				return 0;
			lua_pushnumber(LS, lua_render_ctx->width);
			lua_pushnumber(LS, lua_render_ctx->height);
			lua_pushnumber(LS, lua_render_ctx->rshift);
			lua_pushnumber(LS, lua_render_ctx->gshift);
			lua_pushnumber(LS, lua_render_ctx->bshift);
			return 5;
		}
	} gui_resolution;

	template<uint32_t lua_render_context::*gap>
	class lua_gui_set_gap : public lua_function
	{
	public:
		lua_gui_set_gap(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS, window* win)
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

	class lua_gui_repaint : public lua_function
	{
	public:
		lua_gui_repaint() : lua_function("gui.repaint") {}
		int invoke(lua_State* LS, window* win)
		{
			lua_requests_repaint = true;
			return 0;
		}
	} gui_repaint;

	class lua_gui_update_subframe : public lua_function
	{
	public:
		lua_gui_update_subframe() : lua_function("gui.subframe_update") {}
		int invoke(lua_State* LS, window* win)
		{
			lua_requests_subframe_paint = get_boolean_argument(LS, 1, "gui.subframe_update");
			return 0;
		}
	} gui_update_subframe;
}
