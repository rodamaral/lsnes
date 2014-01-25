#ifndef _library__lua_pin__hpp__included__
#define _library__lua_pin__hpp__included__

#include "lua-base.hpp"

namespace lua
{
/**
 * Pin of an object against Lua GC.
 */
template<typename T> struct objpin
{
/**
 * Create a null pin.
 */
	objpin()
	{
		_state = NULL;
		obj = NULL;
	}
/**
 * Create a new pin.
 *
 * Parameter _state: The state to pin the object in.
 * Parameter _object: The object to pin.
 */
	objpin(state& lstate, T* _object)
		: _state(&lstate.get_master())
	{
		_state->pushlightuserdata(this);
		_state->pushvalue(-2);
		_state->rawset(LUA_REGISTRYINDEX);
		obj = _object;
	}
/**
 * Delete a pin.
 */
	~objpin() throw()
	{
		if(obj) {
			_state->pushlightuserdata(this);
			_state->pushnil();
			_state->rawset(LUA_REGISTRYINDEX);
		}
	}
/**
 * Copy ctor.
 */
	objpin(const objpin& p)
	{
		_state = p._state;
		obj = p.obj;
		if(obj) {
			_state->pushlightuserdata(this);
			_state->pushlightuserdata(const_cast<objpin*>(&p));
			_state->rawget(LUA_REGISTRYINDEX);
			_state->rawset(LUA_REGISTRYINDEX);
		}
	}
/**
 * Assignment operator.
 */
	objpin& operator=(const objpin& p)
	{
		if(_state == p._state && obj == p.obj)
			return *this;
		if(obj == p.obj)
			throw std::logic_error("A Lua object can't be in two lua states at once");
		if(obj) {
			_state->pushlightuserdata(this);
			_state->pushnil();
			_state->rawset(LUA_REGISTRYINDEX);
		}
		_state = p._state;
		if(p.obj) {
			_state->pushlightuserdata(this);
			_state->pushlightuserdata(const_cast<objpin*>(&p));
			_state->rawget(LUA_REGISTRYINDEX);
			_state->rawset(LUA_REGISTRYINDEX);
		}
		obj = p.obj;
		return *this;
	}
/**
 * Clear a pinned object.
 */
	void clear()
	{
		if(obj) {
			_state->pushlightuserdata(this);
			_state->pushnil();
			_state->rawset(LUA_REGISTRYINDEX);
		}
		_state = NULL;
		obj = NULL;
	}
/**
 * Get pointer to pinned object.
 *
 * Returns: The pinned object.
 */
	T* object() { return obj; }
/**
 * Is non-null?
 */
	operator bool() { return obj != NULL; }
/**
 * Smart pointer.
 */
	T& operator*() { if(obj) return *obj; throw std::runtime_error("Attempted to reference NULL Lua pin"); }
	T* operator->() { if(obj) return obj; throw std::runtime_error("Attempted to reference NULL Lua pin"); }
/**
 * Push Lua value.
 */
	void luapush(state& lstate)
	{
		lstate.pushlightuserdata(this);
		lstate.rawget(LUA_REGISTRYINDEX);
	}
private:
	T* obj;
	state* _state;
};

template<typename T> void* unbox_any_pin(struct objpin<T>* pin)
{
	return pin ? pin->object() : NULL;
}

}

#endif
