#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua.hpp"
extern "C"
{
#include <lua.h>
}

template<typename T>
T get_numeric_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
		lua_pushfstring(LS, "argument #%i to %s must be numeric", argindex, fname);
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
		lua_pushfstring(LS, "argument #%i to %s must be numeric if present", argindex, fname);
		lua_error(LS);
	}
	value = static_cast<T>(lua_tonumber(LS, argindex));
}

#endif