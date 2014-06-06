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

void lua_renderq_run(lua::render_context* ctx, void* synchronous_paint_ctx);
uint64_t lua_get_vmabase(const std::string& vma);
uint64_t lua_get_read_address(lua::parameters& P);

#endif
