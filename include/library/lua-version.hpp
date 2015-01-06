#ifndef _library__lua_version__hpp__included__
#define _library__lua_version__hpp__included__

extern "C"
{
#include <lua.h>
}


#if LUA_VERSION_NUM == 501

#else
#if LUA_VERSION_NUM == 502

#define LUA_SUPPORTS_LOAD_MODE
#define LUA_SUPPORTS_RIDX_GLOBALS

#else
#if LUA_VERSION_NUM == 503

#define LUA_SUPPORTS_LOAD_MODE
#define LUA_SUPPORTS_RIDX_GLOBALS
#define LUA_SUPPORTS_INTEGERS
#define LUA_SUPPORTS_LOAD_STRING

#else
#error "Unsupported Lua version"
#endif
#endif
#endif

#ifdef LUA_SUPPORTS_INTEGERS
#define LUA_INTEGER_POSTFIX(X) X##integer
#else
#define LUA_INTEGER_POSTFIX(X) X##number
#endif

#ifdef LUA_SUPPORTS_LOAD_MODE
#define LUA_LOADMODE_ARG(X) , X
#else
#define LUA_LOADMODE_ARG(X)
#endif

#ifdef LUA_SUPPORTS_RIDX_GLOBALS
#define LUA_LOADGLOBALS rawgeti(LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else
#define LUA_LOADGLOBALS pushvalue(LUA_GLOBALSINDEX);
#endif

#ifdef LUA_SUPPORTS_LOAD_STRING
#define LUA_LOAD_CMD "load"
#else
#define LUA_LOAD_CMD "loadstring"
#endif


#endif
