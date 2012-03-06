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
void push_keygroup_parameters(lua_State* LS, const struct keygroup::parameters& p);
extern lua_render_context* lua_render_ctx;
extern controller_frame* lua_input_controllerdata;
extern bool lua_booted_flag;
extern uint64_t lua_idle_hook_time;
extern uint64_t lua_timer_hook_time;

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


template<typename T> struct lua_obj_pin
{
	lua_obj_pin(lua_State* LS, T* _object)
	{
		lua_pushlightuserdata(LS, this);
		lua_pushvalue(LS, -2);
		lua_rawset(LS, LUA_REGISTRYINDEX);
		state = LS;
		obj = _object;
	}
	~lua_obj_pin()
	{
		lua_pushlightuserdata(state, this);
		lua_pushnil(state);
		lua_rawset(state, LUA_REGISTRYINDEX);
	}
	T* object() { return obj; }
private:
	T* obj;
	lua_State* state;
	lua_obj_pin(const lua_obj_pin&);
	lua_obj_pin& operator=(const lua_obj_pin&);
};

template<class T> class lua_class;

template<class T> lua_class<T>& objclass();

template<class T> class lua_class
{
public:
	lua_class(const std::string& _name)
	{
		name = _name;
	}

	template<typename... U> T* _create(lua_State* LS, U... args)
	{
		void* obj = lua_newuserdata(LS, sizeof(T));
		load_metatable(LS);
		lua_setmetatable(LS, -2);
		T* _obj = reinterpret_cast<T*>(obj);
		new(_obj) T(args...);
		return _obj;
	}

	template<typename... U> static T* create(lua_State* LS, U... args)
	{
		return objclass<T>()._create(LS, args...);
	}

	void bind(lua_State* LS)
	{
		load_metatable(LS);
		lua_pushvalue(LS, -3);
		lua_pushvalue(LS, -3);
		lua_rawset(LS, -3);
	}

	void bind(lua_State* LS, const char* keyname)
	{
		load_metatable(LS);
		lua_pushstring(LS, keyname);
		lua_pushvalue(LS, -3);
		lua_rawset(LS, -3);
	}

	T* _get(lua_State* LS, int arg, const char* fname, bool optional = false)
	{
		if(lua_type(LS, arg) == LUA_TNONE || lua_type(LS, arg) == LUA_TNIL) {
			if(optional)
				return NULL;
			else
				goto badtype;
		}
		load_metatable(LS);
		if(!lua_getmetatable(LS, arg))
			goto badtype;
		if(!lua_rawequal(LS, -1, -2))
			goto badtype;
		lua_pop(LS, 2);
		return reinterpret_cast<T*>(lua_touserdata(LS, arg));
badtype:
		char buffer[2048];
		sprintf(buffer, "argument #%i to %s must be %s", arg, fname, name.c_str());
		lua_pushstring(LS, buffer);
		lua_error(LS);
	}

	static T* get(lua_State* LS, int arg, const char* fname, bool optional = false)
	{
		return objclass<T>()._get(LS, arg, fname, optional);
	}

	bool _is(lua_State* LS, int arg)
	{
		if(lua_type(LS, arg) != LUA_TUSERDATA)
			return false;
		load_metatable(LS);
		if(!lua_getmetatable(LS, arg)) {
			lua_pop(LS, 1);
			return false;
		}
		bool ret = lua_rawequal(LS, -1, -2);
		lua_pop(LS, 2);
		return ret;
	}

	static bool is(lua_State* LS, int arg)
	{
		return objclass<T>()._is(LS, arg);
	}

	lua_obj_pin<T>* _pin(lua_State* LS, int arg, const char* fname)
	{
		T* obj = get(LS, arg, fname);
		lua_pushvalue(LS, arg);
		return new lua_obj_pin<T>(LS, obj);
	}

	static lua_obj_pin<T>* pin(lua_State* LS, int arg, const char* fname)
	{
		return objclass<T>()._pin(LS, arg, fname);
	}
private:
	static int dogc(lua_State* LS)
	{
		T* obj = reinterpret_cast<T*>(lua_touserdata(LS, 1));
		obj->~T();
	}

	void load_metatable(lua_State* LS)
	{
again:
		lua_pushlightuserdata(LS, this);
		lua_rawget(LS, LUA_REGISTRYINDEX);
		if(lua_type(LS, -1) == LUA_TNIL) {
			lua_pop(LS, 1);
			lua_pushlightuserdata(LS, this);
			lua_newtable(LS);
			lua_pushstring(LS, "__gc");
			lua_pushcfunction(LS, &lua_class<T>::dogc);
			lua_rawset(LS, -3);
			lua_rawset(LS, LUA_REGISTRYINDEX);
			goto again;
		}
	}
	std::string name;
	lua_class(const lua_class<T>&);
	lua_class& operator=(const lua_class<T>&);
};

#define DECLARE_LUACLASS(x, X) template<> lua_class< x >& objclass() { static lua_class< x > clazz( X ); \
	return clazz; }



#endif
