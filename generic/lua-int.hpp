#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua.hpp"
#include <cstdio>
#include <cstdlib>
extern "C"
{
#include <lua.h>
}

std::string get_string_argument(lua_State* LS, unsigned argindex, const char* fname);
bool get_boolean_argument(lua_State* LS, unsigned argindex, const char* fname);
extern lua_render_context* lua_render_ctx;
extern controls_t* lua_input_controllerdata;


template<typename T>
T get_numeric_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
		char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be numeric", argindex, fname);
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}
	return static_cast<T>(lua_tonumber(LS, argindex));
}

template<typename T>
void get_numeric_argument(lua_State* LS, unsigned argindex, T& value, const char* fname)
{
	if(lua_isnoneornil(LS, argindex))
		return;
	if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
		char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be numeric if present", argindex, fname);
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}
	value = static_cast<T>(lua_tonumber(LS, argindex));
}

#endif
