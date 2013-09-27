#ifndef _lua_int__hpp__included__
#define _lua_int__hpp__included__

#include "lua.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <list>
#include <functional>
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

struct luaclass_methods
{
	bool (*is)(lua_State* LS, int index);
	const std::string& (*name)();
	std::string (*print)(lua_State* LS, int index);
};

std::list<luaclass_methods>& userdata_recogn_fns();
std::string try_recognize_userdata(lua_State* LS, int index);
std::string try_print_userdata(lua_State* LS, int index);

template<typename T>
T get_numeric_argument(lua_State* LS, unsigned argindex, const char* fname)
{
	if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
		static char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be numeric", argindex, fname);
		throw std::runtime_error(buffer);
	}
	return static_cast<T>(lua_tonumber(LS, argindex));
}

template<typename T>
void get_numeric_argument(lua_State* LS, unsigned argindex, T& value, const char* fname)
{
	if(lua_isnoneornil(LS, argindex))
		return;
	if(lua_isnone(LS, argindex) || !lua_isnumber(LS, argindex)) {
		static char buffer[1024];
		sprintf(buffer, "argument #%i to %s must be numeric if present", argindex, fname);
		throw std::runtime_error(buffer);
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
	void luapush(lua_State* LS)
	{
		lua_pushlightuserdata(LS, this);
		lua_rawget(LS, LUA_REGISTRYINDEX);
	}
	T* object() { return obj; }
private:
	T* obj;
	lua_State* state;
	lua_obj_pin(const lua_obj_pin&);
	lua_obj_pin& operator=(const lua_obj_pin&);
};

template<typename T> void* unbox_any_pin(struct lua_obj_pin<T>* pin)
{
	return pin ? pin->object() : NULL;
}

template<class T> struct lua_class_bind_data
{
	int (T::*fn)(lua_State* LS);
};

template<class T> class lua_class;

template<class T> lua_class<T>& objclass();

template<class T> class lua_class
{
public:
	lua_class(const std::string& _name)
	{
		name = _name;
		luaclass_methods m;
		m.is = lua_class<T>::is;
		m.name = lua_class<T>::get_name;
		m.print = lua_class<T>::print;
		userdata_recogn_fns().push_back(m);
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
		lua_pop(LS, 1);
	}

	void bind(lua_State* LS, const char* keyname)
	{
		load_metatable(LS);
		lua_pushstring(LS, keyname);
		lua_pushvalue(LS, -3);
		lua_rawset(LS, -3);
		lua_pop(LS, 1);
	}

	static int class_bind_trampoline(lua_State* LS)
	{
		lua_class_bind_data<T>* b = (lua_class_bind_data<T>*)lua_touserdata(LS, lua_upvalueindex(1));
		const char* fname = lua_tostring(LS, lua_upvalueindex(2));
		try {
			T* p = lua_class<T>::get(LS, 1, fname);
			return (p->*(b->fn))(LS);
		} catch(std::exception& e) {
			lua_pushstring(LS, e.what());
			lua_error(LS);
			return 0;
		}
	}

	void bind(lua_State* LS, const char* keyname, int (T::*fn)(lua_State* LS), bool force = false)
	{
		load_metatable(LS);
		lua_pushstring(LS, keyname);
		lua_rawget(LS, -2);
		if(!lua_isnil(LS, -1) && !force) {
			lua_pop(LS, 2);
			return;
		}
		lua_pop(LS, 1);
		lua_class_bind_data<T>* bdata = new lua_class_bind_data<T>;
		bdata->fn = fn;
		lua_pushstring(LS, keyname);
		lua_pushlightuserdata(LS, bdata);
		std::string name = std::string("Method ") + keyname;
		lua_pushstring(LS, name.c_str());
		lua_pushcclosure(LS, class_bind_trampoline, 2);
		lua_rawset(LS, -3);
		lua_pop(LS, 1);
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
		if(!lua_getmetatable(LS, arg)) {
			goto badtype;
		}
		if(!lua_rawequal(LS, -1, -2)) {
			goto badtype;
		}
		lua_pop(LS, 2);
		return reinterpret_cast<T*>(lua_touserdata(LS, arg));
badtype:
		static char buffer[2048];
		sprintf(buffer, "argument #%i to %s must be %s", arg, fname, name.c_str());
		throw std::runtime_error(buffer);
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

	std::string _recognize(lua_State* LS, int arg)
	{
		return _is(LS, arg) ? name : "";
	}

	static std::string recognize(lua_State* LS, int arg)
	{
		return objclass<T>()._recognize(LS, arg);
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

	static const std::string& get_name()
	{
		return objclass<T>().name;
	}

	static std::string print(lua_State* LS, int index)
	{
		T* obj = get(LS, index, "__internal_print");
		return obj->print();
	}
private:
	static int dogc(lua_State* LS)
	{
		T* obj = reinterpret_cast<T*>(lua_touserdata(LS, 1));
		obj->~T();
	}

	static int newindex(lua_State* LS)
	{
		lua_pushstring(LS, "Writing metatables of classes is not allowed");
		lua_error(LS);
	}

	static int index(lua_State* LS)
	{
		lua_getmetatable(LS, 1);
		lua_pushvalue(LS, 2);
		lua_rawget(LS, -2);
		return 1;
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
			lua_pushvalue(LS, -1);
			lua_setmetatable(LS, -2);
			lua_pushstring(LS, "__gc");
			lua_pushcfunction(LS, &lua_class<T>::dogc);
			lua_rawset(LS, -3);
			lua_pushstring(LS, "__newindex");
			lua_pushcfunction(LS, &lua_class<T>::newindex);
			lua_rawset(LS, -3);
			lua_pushstring(LS, "__index");
			lua_pushcfunction(LS, &lua_class<T>::index);
			lua_rawset(LS, -3);
			lua_rawset(LS, LUA_REGISTRYINDEX);
			goto again;
		}
	}
	std::string name;
	lua_class(const lua_class<T>&);
	lua_class& operator=(const lua_class<T>&);
};

bool lua_do_once(lua_State* LS, void* key);

#define DECLARE_LUACLASS(x, X) template<> lua_class< x >& objclass() { static lua_class< x > clazz( X ); \
	return clazz; }



#endif
