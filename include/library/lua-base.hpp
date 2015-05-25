#ifndef _library__lua_base__hpp__included__
#define _library__lua_base__hpp__included__

#include <string>
#include <stdexcept>
#include <typeinfo>
#include <typeindex>
#include <map>
#include <unordered_map>
#include <set>
#include <list>
#include <cassert>
#include "string.hpp"
#include "utf8.hpp"
#include "lua-version.hpp"

namespace lua
{
class state;
class function;
class function_group;
class class_group;
class class_base;

/**
 * Lua state object.
 */
class state
{
public:
	const static unsigned trampoline_upvals = 2;
	//Auxillary type for store-tag.
	template<typename T> struct _store_tag
	{
		T& addr;
		T val;
		_store_tag(T& a, T v) : addr(a), val(v) {}
	};
	//Auxillary type for vararg-tag.
	struct vararg_tag
	{
		std::list<std::string> args;
		vararg_tag(std::list<std::string>& _args) : args(_args) {}
		int pushargs(state& L);
	};

	//Auxillary type for numeric-tag.
	template<typename T> struct _numeric_tag
	{
		T val;
		_numeric_tag(T v) : val(v) {}
	};

	//Auxillary type for fnptr-tag.
	template<typename T> struct _fnptr_tag
	{
		int(*fn)(state& L, T v);
		T val;
		_fnptr_tag(int (*f)(state& L, T v), T v) : fn(f), val(v) {}
	};

	//Auxillary type for fn-tag.
	template<typename T> struct _fn_tag
	{
		T fn;
		_fn_tag(T f) : fn(f) {}
	};

	/**
	 * Callback parameter: Don't pass any real parameter, but instead store specified value in specified
	 * location.
	 *
	 * Parameter a: The location to store value to.
	 * Parameter v: The value to store.
	 * Returns: The parameter structure.
	 */
	template<typename T> static struct _store_tag<T> store_tag(T& a, T v) { return _store_tag<T>(a, v); }
	/**
	 * Callback parameter: Pass numeric value.
	 *
	 * Parameter v: The value to pass.
	 * Returns: The parameter structure.
	 */
	template<typename T> static struct _numeric_tag<T> numeric_tag(T v) { return _numeric_tag<T>(v); }

	/**
	 * Callback parameter: Execute function to push more parameters.
	 *
	 * Parameter f: The function to execute. The return value is number of additional parameters pushed.
	 * Parameter v: The value to pass to function.
	 * Returns: The parameter structure.
	 */
	template<typename T> static struct _fnptr_tag<T> fnptr_tag(int (*f)(state& L, T v), T v)
	{
		return _fnptr_tag<T>(f, v);
	}

	/**
	 * Callback parameter: Execute function to push more parameters.
	 *
	 * Parameter v: The functor to execute. Passed reference to the Lua state. The return value is number of
	 *	additional parameters pushed.
	 * Returns: The parameter structure.
	 */
	template<typename T> static struct _fn_tag<T> fn_tag(T v) { return _fn_tag<T>(v); }

	/**
	 * Callback parameter: Pass boolean argument.
	 *
	 * Parameter v: The boolean value to pass.
	 */
	struct boolean_tag { bool val; boolean_tag(bool v) : val(v) {}};

	/**
	 * Callback parameter: Pass string argument.
	 *
	 * Parameter v: The string value to pass.
	 */
	struct string_tag { std::string val; string_tag(const std::string& v) : val(v) {}};

	/**
	 * Callback parameter: Pass nil argument.
	 */
	struct nil_tag { nil_tag() {}};
private:
	template<typename U, typename... T> void _callback(int argc, _store_tag<U> tag, T... args)
	{
		tag.addr = tag.val;
		_callback(argc, args...);
		tag.addr = NULL;
	}

	template<typename... T> void _callback(int argc, vararg_tag tag, T... args)
	{
		int e = tag.pushargs(*this);
		_callback(argc + e, args...);
	}

	template<typename... T> void _callback(int argc, nil_tag tag, T... args)
	{
		pushnil();
		_callback(argc + 1, args...);
	}

	template<typename... T> void _callback(int argc, boolean_tag tag, T... args)
	{
		pushboolean(tag.val);
		_callback(argc + 1, args...);
	}

	template<typename... T> void _callback(int argc, string_tag tag, T... args)
	{
		pushlstring(tag.val);
		_callback(argc + 1, args...);
	}

	template<typename U, typename... T> void _callback(int argc, _numeric_tag<U> tag, T... args)
	{
		pushnumber(tag.val);
		_callback(argc + 1, args...);
	}

	template<typename U, typename... T> void _callback(int argc, _fnptr_tag<U> tag, T... args)
	{
		int extra = tag.fn(*this, tag.val);
		_callback(argc + extra, args...);
	}

	template<typename U, typename... T> void _callback(int argc, _fn_tag<U> tag, T... args)
	{
		int extra = tag.fn(*this);
		_callback(argc + extra, args...);
	}

	void _callback(int argc)
	{
		int r = pcall(argc, 0, 0);
		if(r == LUA_ERRRUN) {
			(stringfmt() << "Error running Lua callback: " << tostring(-1)).throwex();
			pop(1);
		}
		if(r == LUA_ERRMEM) {
			(stringfmt() << "Error running Lua callback: Out of memory").throwex();
			pop(1);
		}
		if(r == LUA_ERRERR) {
			(stringfmt() << "Error running Lua callback: Double Fault???").throwex();
			pop(1);
		}
	}
public:
/**
 * Create a new state.
 */
	state() throw(std::bad_alloc);
/**
 * Create a new state with specified master state.
 */
	state(state& _master, lua_State* L);
/**
 * Destroy a state.
 */
	~state() throw();
/**
 * Get the internal state object.
 *
 * Return value: Internal state.
 */
	lua_State* handle() { return lua_handle; }
/**
 * Get the master state.
 */
	state& get_master() { return master ? master->get_master() : *this; }
/**
 * Set the internal state object.
 */
	void handle(lua_State* l) { lua_handle = l; }
/**
 * Set OOM handler.
 */
	void set_oom_handler(void (*oom)()) { oom_handler = oom ? oom : builtin_oom; }
/**
 * Set soft OOM handler.
 */
	void set_soft_oom_handler(void (*oom)(int status)) { soft_oom_handler = oom ? oom : builtin_soft_oom; }
/**
 * Reset the state.
 */
	void reset() throw(std::runtime_error, std::bad_alloc);
/**
 * Deinit the state.
 */
	void deinit() throw();
/**
 * Create a trampoline.
 *
 * Parameter fn The function to execute.
 * Parameter n_upvals The number of extra upvalues besides the 2 used by trampoline itself. Popped from stack.
 */
	void push_trampoline(int(*fn)(state& L), unsigned n_upvals);
/**
 * Get specified trampoline upvalue index.
 */
	int trampoline_upval(int val) { return lua_upvalueindex(trampoline_upvals + val); }
/**
 * Set value of interruptable flag.
 *
 * Parameter flag: The flag.
 */
	void set_interruptable_flag(bool flag)
	{
		if(master) master->set_interruptable_flag(flag); else interruptable = flag;
	}
/**
 * Get interruptable flag.
 */
	bool get_interruptable_flag()
	{
		if(master) return master->get_interruptable_flag(); else return interruptable;
	}
/**
 * Set memory limit.
 */
	void set_memory_limit(size_t limit)
	{
		if(master) master->set_memory_limit(limit); else memory_limit = limit;
	}
/**
 * Get memory limit.
 */
	size_t get_memory_limit()
	{
		if(master) return master->get_memory_limit(); else return memory_limit;
	}
/**
 * Get memory use.
 */
	size_t get_memory_use()
	{
		if(master) return master->get_memory_use(); else return memory_use;
	}
/**
 * Charge against memory limit.
 */
	bool charge_memory(size_t amount, bool release);
/**
 * Execute function in interruptable mode.
 *
 * Parameter fn: The function to execute
 * Parameter in: Number of slots to copy in.
 * Parameter out: Number of slots to copy out.
 */
	void run_interruptable(std::function<void()> fn, unsigned in, unsigned out);
/**
 * Get a string argument.
 *
 * Parameter argindex: The stack index.
 * Parameter fname: The name of function to use in error messages.
 * Returns: The string.
 * Throws std::runtime_error: The specified argument is not a string.
 */
	std::string get_string(int argindex, const std::string& fname) throw(std::runtime_error, std::bad_alloc)
	{
		if(isnone(argindex))
			(stringfmt() << "argument #" << argindex << " to " << fname << " must be string").throwex();
		size_t len;
		const char* f = lua_tolstring(lua_handle, argindex, &len);
		if(!f)
			(stringfmt() << "argument #" << argindex << " to " << fname << " must be string").throwex();
		return std::string(f, f + len);
	}
/**
 * Get a boolean argument.
 *
 * Parameter argindex: The stack index.
 * Parameter fname: The name of function to use in error messages.
 * Returns: The string.
 * Throws std::runtime_error: The specified argument is not a boolean.
 */
	bool get_bool(int argindex, const std::string& fname) throw(std::runtime_error, std::bad_alloc)
	{
		if(isnone(argindex) || !isboolean(argindex))
			(stringfmt() << "argument #" << argindex << " to " << fname << " must be boolean").throwex();
		return (lua_toboolean(lua_handle, argindex) != 0);
	}
/**
 * Get a mandatory numeric argument.
 *
 * Parameter argindex: The stack index.
 * Parameter fname: The name of function to use in error messages.
 * Returns: The parsed number.
 * Throws std::runtime_error: Bad type.
 */
	template<typename T>
	T get_numeric_argument(int argindex, const std::string& fname)
	{
		if(std::numeric_limits<T>::is_integer) {
			if(isnone(argindex) || !isinteger(argindex))
				(stringfmt() << "Argument #" << argindex << " to " << fname
					<< " must be integer").throwex();
			return static_cast<T>(lua_tointeger(lua_handle, argindex));
		} else {
			if(isnone(argindex) || !isnumber(argindex))
				(stringfmt() << "Argument #" << argindex << " to " << fname
					<< " must be numeric").throwex();
			return static_cast<T>(lua_tonumber(lua_handle, argindex));
		}
	}
/**
 * Get a optional numeric argument.
 *
 * Parameter argindex: The stack index.
 * Parameter value: The place to store the value.
 * Parameter fname: The name of function to use in error messages.
 * Throws std::runtime_error: Bad type.
 */
	template<typename T>
	void get_numeric_argument(unsigned argindex, T& value, const std::string& fname)
	{
		if(isnoneornil(argindex))
			return;
		if(std::numeric_limits<T>::is_integer) {
			if(isnone(argindex) || !isinteger(argindex))
				(stringfmt() << "Argument #" << argindex << " to " << fname << " must be integer if "
					"present").throwex();
			value = static_cast<T>(lua_tointeger(lua_handle, argindex));
		} else {
			if(isnone(argindex) || !isnumber(argindex))
				(stringfmt() << "Argument #" << argindex << " to " << fname << " must be numeric if "
					"present").throwex();
			value = static_cast<T>(lua_tonumber(lua_handle, argindex));
		}
	}
/**
 * Do a callback.
 *
 * Parameter name: The name of the callback.
 * Parameter args: Arguments to pass to the callback.
 */
	template<typename... T>
	bool callback(const std::string& name, T... args)
	{
		getglobal(name.c_str());
		int t = type(-1);
		if(t != LUA_TFUNCTION) {
			pop(1);
			return false;
		}
		_callback(0, args...);
		return true;
	}
/**
 * Do a callback.
 *
 * Parameter cblist: List of environment keys to do callbacks.
 * Parameter args: Arguments to pass to the callback.
 */
	template<typename... T>
	bool callback(const std::list<char>& cblist, T... args)
	{
		bool any = false;
		for(auto& i : cblist) {
			pushlightuserdata(const_cast<char*>(&i));
			rawget(LUA_REGISTRYINDEX);
			int t = type(-1);
			if(t != LUA_TFUNCTION) {
				pop(1);
			} else {
				_callback(0, args...);
				any = true;
			}
		}
		return any;
	}
/**
 * Add a group of functions.
 */
	void add_function_group(function_group& group);
/**
 * Add a group of classes.
 */
	void add_class_group(class_group& group);
/**
 * Function callback.
 */
	void function_callback(const std::string& name, function* func);
/**
 * Class callback.
 */
	void class_callback(const std::string& name, class_base* func);
/**
 * Do something just once per VM.
 *
 * Parameter key: The do-once key value.
 * Returns: True if called the first time for given key on given VM, false otherwise.
 */
	bool do_once(void* key);
/**
 * Callback list.
 */
	class callback_list
	{
	public:
		callback_list(state& L, const std::string& name, const std::string& fn_cbname = "");
		~callback_list();
		void _register(state& L);	//Reads callback from top of lua stack.
		void _unregister(state& L);	//Reads callback from top of lua stack.
		template<typename... T> bool callback(T... args) {
			bool any = L.callback(callbacks, args...);
			if(fn_cbname != "" && L.callback(fn_cbname, args...))
				any = true;
			return any;
		}
		const std::string& get_name() { return name; }
		void clear() { callbacks.clear(); }
	private:
		callback_list(const callback_list&);
		callback_list& operator=(const callback_list&);
		std::list<char> callbacks;
		state& L;
		std::string name;
		std::string fn_cbname;
	};
/**
 * Enumerate all callbacks.
 */
	std::list<callback_list*> get_callbacks();
/**
 * Register/Unregister a callback list.
 */
	void do_register(const std::string& name, callback_list& callback);
	void do_unregister(const std::string& name, callback_list& callback);

	//All kinds of Lua API functions.
	void pop(int n) { lua_pop(lua_handle, n); }
	void* newuserdata(size_t size) { return lua_newuserdata(lua_handle, size); }
	int setmetatable(int index) { return lua_setmetatable(lua_handle, index); }
	int type(int index) { return lua_type(lua_handle, index); }
	void replace(int index) { lua_replace(lua_handle, index); }
	int getmetatable(int index) { return lua_getmetatable(lua_handle, index); }
	int rawequal(int index1, int index2) { return lua_rawequal(lua_handle, index1, index2); }
	void* touserdata(int index) { return lua_touserdata(lua_handle, index); }
	const void* topointer(int index) { return lua_topointer(lua_handle, index); }
	int gettop() { return lua_gettop(lua_handle); }
	void pushvalue(int index) { lua_pushvalue(lua_handle, index); }
	void pushlightuserdata(void* p) { lua_pushlightuserdata(lua_handle, p); }
	void rawset(int index) { lua_rawset(lua_handle, index); }
	void pushnil() { lua_pushnil(lua_handle); }
	void pushstring(const char* s) { lua_pushstring(lua_handle, s); }
	void rawget(int index) { lua_rawget(lua_handle, index); }
	int isnil(int index) { return lua_isnil(lua_handle, index); }
	void newtable() { lua_newtable(lua_handle); }
	void pushcclosure(lua_CFunction fn, int n) { lua_pushcclosure(lua_handle, fn, n); }
	void pushcfunction(lua_CFunction fn) { lua_pushcfunction(lua_handle, fn); }
	void setfield(int index, const char* k) { lua_setfield(lua_handle, index, k); }
	void getfield(int index, const char* k) { lua_getfield(lua_handle, index, k); }
	void getglobal(const char* name) { lua_getglobal(lua_handle, name); }
	void setglobal(const char* name) { lua_setglobal(lua_handle, name); }
	void insert(int index) { lua_insert(lua_handle, index); }
	void settable(int index) { lua_settable(lua_handle, index); }
	int isnone(int index) { return lua_isnone(lua_handle, index); }
	int isnumber(int index) { return lua_isnumber(lua_handle, index); }
	int isinteger(int index) { return LUA_INTEGER_POSTFIX(lua_is) (lua_handle, index); }
	int isboolean(int index) { return lua_isboolean(lua_handle, index); }
	int toboolean(int index) { return lua_toboolean(lua_handle, index); }
	const char* tolstring(int index, size_t *len) { return lua_tolstring(lua_handle, index, len); }
	lua_Number tonumber(int index) { return lua_tonumber(lua_handle, index); }
	uint64_t tointeger(int index) { return LUA_INTEGER_POSTFIX(lua_to) (lua_handle, index); }
	void gettable(int index) { lua_gettable(lua_handle, index); }
	int load(lua_Reader reader, void* data, const char* chunkname, const char* mode) {
		(void)mode;
		return lua_load(lua_handle, reader, data, chunkname LUA_LOADMODE_ARG(mode) );
	}
	const char* tostring(int index) { return lua_tostring(lua_handle, index); }
	const char* tolstring(int index, size_t& len) { return lua_tolstring(lua_handle, index, &len); }
	void pushlstring(const char* s, size_t len) { lua_pushlstring(lua_handle, s, len); }
	void pushlstring(const std::string& s) { lua_pushlstring(lua_handle, s.c_str(), s.length()); }
	void pushlstring(const char32_t* s, size_t len) { pushlstring(utf8::to8(std::u32string(s, len))); }
	int pcall(int nargs, int nresults, int errfunc)
	{
		state* master_state = this;
		while(master_state->master) master_state = master_state->master;
		//Upon entry to protected mode, interruptable mode is always set, and it is restored on exit
		//from protected mode.
		bool old_interruptable = master_state->interruptable;
		master_state->interruptable = true;
		auto ret = lua_pcall(lua_handle, nargs, nresults, errfunc);
		master_state->interruptable = old_interruptable;
		return ret;
	}
	int next(int index) { return lua_next(lua_handle, index); }
	int isnoneornil(int index) { return lua_isnoneornil(lua_handle, index); }
	void rawgeti(int index, int n) { lua_rawgeti(lua_handle, index, n); }
	template<typename T> void pushnumber(T val)
	{
		if(std::numeric_limits<T>::is_integer)
			_pushinteger(val);
		else
			_pushnumber(val);
	}
	void pushboolean(bool b) { lua_pushboolean(lua_handle, b); }
	void pushglobals() { LUA_LOADGLOBALS }
private:
	void _pushnumber(lua_Number n) { return lua_pushnumber(lua_handle, n); }
	void _pushinteger(uint64_t n) { return LUA_INTEGER_POSTFIX(lua_push) (lua_handle, n); }
	static void builtin_oom();
	static void builtin_soft_oom(int status);
	static void* builtin_alloc(void* user, void* old, size_t olds, size_t news);
	void (*oom_handler)();
	void (*soft_oom_handler)(int status);
	state* master;
	bool interruptable;
	size_t memory_limit;
	size_t memory_use;
	lua_State* lua_handle;
	state(state&);
	state& operator=(state&);
};

}

#endif
