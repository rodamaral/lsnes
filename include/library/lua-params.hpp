#ifndef _library__lua_params__hpp__included__
#define _library__lua_params__hpp__included__

#include "lua-base.hpp"
#include "lua-framebuffer.hpp"
#include "lua-class.hpp"

namespace lua
{
struct function_parameter_tag
{
	function_parameter_tag(int& _idx) : idx(_idx) {}
	int& idx;
};

struct table_parameter_tag
{
	table_parameter_tag(int& _idx) : idx(_idx) {}
	int& idx;
};

template<typename T, typename U> struct optional_parameter_tag
{
	optional_parameter_tag(T& _target, U _dflt) : target(_target), dflt(_dflt) {}
	T& target;
	U dflt;
};

struct skipped_parameter_tag
{
};

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

template<> void arg_helper(state& L, framebuffer::color& x, int idx, const std::string& fname)
{
	x = get_fb_color(L, idx, fname);
}

template<> void arg_helper(state& L, skipped_parameter_tag& x, int idx, const std::string& fname)
{
	delete &x;
}

template<> void arg_helper(state& L, function_parameter_tag& x, int idx, const std::string& fname)
{
	if(L.type(idx) != LUA_TFUNCTION)
		(stringfmt() << "Expected function as argument #" << idx << " to " << fname).throwex();
	x.idx = idx;
	delete &x;
}

template<> void arg_helper(state& L, table_parameter_tag& x, int idx, const std::string& fname)
{
	if(L.type(idx) != LUA_TTABLE)
		(stringfmt() << "Expected table as argument #" << idx << " to " << fname).throwex();
	x.idx = idx;
	delete &x;
}

template<typename T, typename U> void arg_helper(state& L, optional_parameter_tag<T, U>& x, int idx,
	const std::string& fname)
{
	x.target = x.dflt;
	L.get_numeric_argument<T>(idx, x.target, fname);
	delete &x;
}

template<typename U> void arg_helper(state& L, optional_parameter_tag<bool, U>& x, int idx, const std::string& fname)
{
	x.target = (L.type(idx) == LUA_TNIL || L.type(idx) == LUA_TNONE) ? x.dflt : L.get_bool(idx, fname);
	delete &x;
}

template<typename U> void arg_helper(state& L, optional_parameter_tag<std::string, U>& x, int idx,
	const std::string& fname)
{
	x.target = (L.type(idx) == LUA_TNIL || L.type(idx) == LUA_TNONE) ? x.dflt : L.get_string(idx, fname);
	delete &x;
}

template<typename U> void arg_helper(state& L, optional_parameter_tag<framebuffer::color, U>& x, int idx,
	const std::string& fname)
{
	x.target = get_fb_color(L, idx, fname, x.dflt);
	delete &x;
}

template<typename T, typename U> void arg_helper(state& L, optional_parameter_tag<T*, U>& x, int idx,
	const std::string& fname)
{
	x.target = _class<T>::get(L, idx, fname, true);
	delete &x;
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
		arg_helper(L, optional<T>(tmp, d), i ? i : next, fname);
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
/**
 * Read multiple at once.
 */
	template<typename T, typename... U> void operator()(T& x, U&... args)
	{
		arg_helper(L, x, next, fname);
		next++;
		(*this)(args...);
	}
	void operator()()
	{
	}
/**
 * Optional tag.
 */
	template<typename T, typename U> optional_parameter_tag<T, U>& optional(T& value, U dflt)
	{
		return *new optional_parameter_tag<T, U>(value, dflt);
	}
/**
 * Optional tag, reference default value.
 */
	template<typename T, typename U> optional_parameter_tag<T, const U&>& optional2(T& value, const U& dflt)
	{
		return *new optional_parameter_tag<T, const U&>(value, dflt);
	}
/**
 * Skipped tag.
 */
	skipped_parameter_tag& skipped() { return *new skipped_parameter_tag(); }
/**
 * Function tag.
 */
	function_parameter_tag& function(int& fnidx) { return *new function_parameter_tag(fnidx); }
/**
 * Table tag.
 */
	table_parameter_tag& table(int& fnidx) { return *new table_parameter_tag(fnidx); }
/**
 * Get Lua state.
 */
	state& get_state() { return L; }
private:
	state& L;
	std::string fname;
	int next;
};
}

#endif
