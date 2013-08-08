#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua/lua.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "library/luabase.hpp"

extern lua_state lsnes_lua_state;
extern lua_function_group lua_func_bit;
extern lua_function_group lua_func_misc;
extern lua_function_group lua_func_callback;
extern lua_function_group lua_func_load;

void push_keygroup_parameters(lua_state& L, keyboard_key& p);
extern lua_render_context* lua_render_ctx;
extern controller_frame* lua_input_controllerdata;
extern bool* lua_kill_frame;
extern bool lua_booted_flag;
extern uint64_t lua_idle_hook_time;
extern uint64_t lua_timer_hook_time;

#endif
