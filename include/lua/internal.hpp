#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua/lua.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "library/lua-base.hpp"
#include "library/lua-framebuffer.hpp"

extern lua::state lsnes_lua_state;
extern lua::function_group lua_func_bit;
extern lua::function_group lua_func_misc;
extern lua::function_group lua_func_callback;
extern lua::function_group lua_func_load;
extern lua::function_group lua_func_zip;

void push_keygroup_parameters(lua::state& L, keyboard::key& p);
extern lua_render_context* lua_render_ctx;
extern controller_frame* lua_input_controllerdata;
extern bool* lua_kill_frame;
extern bool lua_booted_flag;
extern uint64_t lua_idle_hook_time;
extern uint64_t lua_timer_hook_time;

extern void* synchronous_paint_ctx;
void lua_renderq_run(lua_render_context* ctx, void* synchronous_paint_ctx);

#endif
