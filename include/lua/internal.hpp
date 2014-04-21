#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua/lua.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "library/lua-base.hpp"
#include "library/lua-class.hpp"
#include "library/lua-function.hpp"
#include "library/lua-params.hpp"
#include "library/lua-framebuffer.hpp"

extern lua::state lsnes_lua_state;
extern lua::function_group lua_func_bit;
extern lua::function_group lua_func_misc;
extern lua::function_group lua_func_load;
extern lua::function_group lua_func_zip;

extern lua::class_group lua_class_callback;
extern lua::class_group lua_class_gui;
extern lua::class_group lua_class_bind;
extern lua::class_group lua_class_pure;
extern lua::class_group lua_class_movie;
extern lua::class_group lua_class_memory;
extern lua::class_group lua_class_fileio;

void push_keygroup_parameters(lua::state& L, keyboard::key& p);
extern lua_render_context* lua_render_ctx;
extern controller_frame* lua_input_controllerdata;
extern bool* lua_kill_frame;
extern uint32_t* lua_hscl;
extern uint32_t* lua_vscl;
extern uint64_t lua_idle_hook_time;
extern uint64_t lua_timer_hook_time;

extern void* synchronous_paint_ctx;
void lua_renderq_run(lua_render_context* ctx, void* synchronous_paint_ctx);
uint64_t lua_get_vmabase(const std::string& vma);
uint64_t lua_get_read_address(lua::parameters& P);

#endif
