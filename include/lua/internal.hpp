#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua/lua.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "library/lua.hpp"

extern lua_state LS;

void push_keygroup_parameters(lua_state& L, const struct keygroup::parameters& p);
extern lua_render_context* lua_render_ctx;
extern controller_frame* lua_input_controllerdata;
extern bool lua_booted_flag;
extern uint64_t lua_idle_hook_time;
extern uint64_t lua_timer_hook_time;

#endif
