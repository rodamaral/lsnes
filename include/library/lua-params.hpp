#ifndef _library__lua_params__hpp__included__
#define _library__lua_params__hpp__included__

#include "lua-base.hpp"
#include "lua-framebuffer.hpp"
#include "lua-function.hpp"

namespace lua
{
template<typename T> static void arg_helper(state& L, T& x, int idx, const std::string& fname)
{
	x = L.get_numeric_argument<T>(idx, fname);
}

template<> void arg_helper(state& L, bool& x, int idx, const std::string& fname)
{
	x = L.get_bool(idx, fname);
}

template<> void arg_helper(state& L, std::string& x, int idx, const std::string& fname)
{
	x = L.get_string(idx, fname);
}

template<typename T> void arg_helper(state& L, T*& x, int idx, const std::string& fname)
{
	x = _class<T>::get(L, idx, fname);
}

template<typename T> void arg_helper(state& L, lua::objpin<T>& x, int idx, const std::string& fname)
{
	x = _class<T>::pin(L, idx, fname);
}

template<typename T> static void arg_helper_opt(state& L, T& x, T dflt, int idx, const std::string& fname)
{
	x = dflt;
	L.get_numeric_argument<T>(idx, x, fname);
}

template<> void arg_helper_opt(state& L, bool& x, bool dflt, int idx, const std::string& fname)
{
	x = (L.type(idx) == LUA_TNIL || L.type(idx) == LUA_TNONE) ? dflt : L.get_bool(idx, fname);
}

template<> void arg_helper_opt(state& L, std::string& x, std::string dflt, int idx, const std::string& fname)
{
	x = (L.type(idx) == LUA_TNIL || L.type(idx) == LUA_TNONE) ? dflt : L.get_string(idx, fname);
}

template<typename T> void arg_helper_opt(state& L, T*& x, T* dflt, int idx, const std::string& fname)
{
	x = _class<T>::get(L, idx, fname, true);
}

/**
 * Parameters for Lua function.
 */
class parameters
{
public:
/**
 * Make
 */
	parameters(state& _L, const std::string& _fname)
		: L(_L), fname(_fname), next(1)
	{
	}
/**
 * Read mandatory argument.
 *
 * Parameter i: Index to read. If 0, read next and advance pointer.
 * Returns: The read value.
 *
 * Notes: The following types can be read:
 * - Numeric types.
 * - Pointers to lua classes.
 * - Pins of lua classes.
 */
	template<typename T> T arg(int i = 0)
	{
		T tmp;
		arg_helper(L, tmp, i ? i : next, fname);
		if(!i) next++;
		return tmp;
	}
/**
 * Read optional argument.
 *
 * Parameter d: The default value.
 * Parameter i: Index to read. If 0, read next and advance pointer.
 * Returns: The read value.
 *
 * Notes: The following types can be read:
 * - Numeric types.
 * - Pointers to lua classes (d is ignored, assumed NULL).
 */
	template<typename T> T arg_opt(T d, int i = 0)
	{
		T tmp;
		arg_helper_opt(L, tmp, d, i ? i : next, fname);
		if(!i) next++;
		return tmp;
	}
/**
 * Get color.
 */
	framebuffer::color color(int64_t d, int i = 0)
	{
		framebuffer::color tmp;
		tmp = lua_get_fb_color(L, i ? i : next, fname, d);
		if(!i) next++;
		return tmp;
	}
/**
 * Is of specified class?
 *
 * Parameter i: Index to read. If 0, read next.
 * Returns: True if it is, false if is is not.
 */
	template<typename T> bool is(int i = 0)
	{
		return _class<T>::is(L, i ? i : next);
	}
/**
 * Skip argument and return index.
 *
 * Returns: The index.
 */
	int skip() { return next++; }
/**
 * Reset sequence.
 */
	void reset(int idx = 1) { next = idx; }
/**
 * Get name.
 */
	const std::string& get_fname() { return fname; }
/**
 * More arguments remain?
 */
	bool more() { return (L.type(next) != LUA_TNONE); }
/**
 * Is of lua type?
 */
	bool is_novalue(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TNONE || t == LUA_TNIL); }
	bool is_none(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TNONE); }
	bool is_nil(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TNIL); }
	bool is_boolean(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TBOOLEAN); }
	bool is_number(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TNUMBER); }
	bool is_string(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TSTRING); }
	bool is_thread(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TTHREAD); }
	bool is_table(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TTABLE); }
	bool is_function(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TFUNCTION); }
	bool is_lightuserdata(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TLIGHTUSERDATA); }
	bool is_userdata(int i = 0) { int t = L.type(i ? i : next); return (t == LUA_TUSERDATA); }
/**
 * Throw an error.
 */
	void expected(const std::string& what, int i = 0)
	{
		(stringfmt() << "Expected " << what << " as argument #" << (i ? i : next) << " of "
			<< fname).throwex();
	}
private:
	state& L;
	std::string fname;
	int next;
};

/**
 * Register function pointer as lua function.
 */
class fnptr2 : public function
{
public:
/**
 * Register.
 */
	fnptr2(function_group& group, const std::string& name, int (*_fn)(state& L, parameters& P))
		: function(group, name)
	{
		fn = _fn;
	}
/**
 * Invoke function.
 */
	int invoke(state& L)
	{
		parameters P(L, fname);
		return fn(L, P);
	}
private:
	int (*fn)(state& L, parameters& P);
};

}

#endif
