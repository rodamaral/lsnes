#ifndef _library__luabase__hpp__included__
#define _library__luabase__hpp__included__

#include <string>
#include <stdexcept>
#include <map>
#include <set>
#include <list>
#include <cassert>
#include "string.hpp"
#include "utf8.hpp"
extern "C"
{
#include <lua.h>
}

class lua_state;

struct luaclass_methods
{
	bool (*is)(lua_state& state, int index);
	const std::string& (*name)();
	std::string (*print)(lua_state& state, int index);
};

std::list<luaclass_methods>& userdata_recogn_fns();
std::string try_recognize_userdata(lua_state& state, int index);
std::string try_print_userdata(lua_state& state, int index);

struct lua_function;

/**
 * Group of functions.
 */
class lua_function_group
{
public:
/**
 * Create a group.
 */
	lua_function_group();
/**
 * Destroy a group.
 */
	~lua_function_group();
/**
 * Add a function to group.
 */
	void do_register(const std::string& name, lua_function& fun);
/**
 * Drop a function from group.
 */
	void do_unregister(const std::string& name);
/**
 * Request callbacks on all currently registered functions.
 */
	void request_callback(std::function<void(std::string, lua_function*)> cb);
/**
 * Bind a callback.
 *
 * Callbacks for all registered functions are immediately called.
 */
	int add_callback(std::function<void(std::string, lua_function*)> cb,
		std::function<void(lua_function_group*)> dcb);
/**
 * Unbind a calback.
 */
	void drop_callback(int handle);
private:
	int next_handle;
	std::map<std::string, lua_function*> functions;
	std::map<int, std::function<void(std::string, lua_function*)>> callbacks;
	std::map<int, std::function<void(lua_function_group*)>> dcallbacks;
};

/**
 * Lua state object.
 */
class lua_state
{
public:
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
		int pushargs(lua_state& L);
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
		int(*fn)(lua_state& L, T v);
		T val;
		_fnptr_tag(int (*f)(lua_state& L, T v), T v) : fn(f), val(v) {}
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
	template<typename T> static struct _fnptr_tag<T> fnptr_tag(int (*f)(lua_state& L, T v), T v)
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
		pushlstring(tag.val.c_str(), tag.val.length());
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
	lua_state() throw(std::bad_alloc);
/**
 * Create a new state with specified master state.
 */
	lua_state(lua_state& _master, lua_State* L);
/**
 * Destroy a state.
 */
	~lua_state() throw();
/**
 * Get the internal state object.
 *
 * Return value: Internal state.
 */
	lua_State* handle() { return lua_handle; }
/**
 * Get the master state.
 */
	lua_state& get_master() { return master ? master->get_master() : *this; }
/**
 * Set the internal state object.
 */
	void handle(lua_State* l) { lua_handle = l; }
/**
 * Set OOM handler.
 */
	void set_oom_handler(void (*oom)()) { oom_handler = oom ? oom : builtin_oom; }
/**
 * Reset the state.
 */
	void reset() throw(std::runtime_error, std::bad_alloc);
/**
 * Deinit the state.
 */
	void deinit() throw();
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
		if(lua_isnone(lua_handle, argindex))
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
		if(lua_isnone(lua_handle, argindex) || !lua_isboolean(lua_handle, argindex))
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
		if(lua_isnone(lua_handle, argindex) || !lua_isnumber(lua_handle, argindex))
			(stringfmt() << "Argunment #" << argindex << " to " << fname << " must be numeric").throwex();
		return static_cast<T>(lua_tonumber(lua_handle, argindex));
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
		if(lua_isnoneornil(lua_handle, argindex))
			return;
		if(lua_isnone(lua_handle, argindex) || !lua_isnumber(lua_handle, argindex))
			(stringfmt() << "Argunment #" << argindex << " to " << fname << " must be numeric if "
				"present").throwex();
		value = static_cast<T>(lua_tonumber(lua_handle, argindex));
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
	void add_function_group(lua_function_group& group);
/**
 * Function callback.
 */
	void function_callback(const std::string& name, lua_function* func);
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
	class lua_callback_list
	{
	public:
		lua_callback_list(lua_state& L, const std::string& name, const std::string& fn_cbname = "");
		~lua_callback_list();
		void _register(lua_state& L);	//Reads callback from top of lua stack.
		void _unregister(lua_state& L);	//Reads callback from top of lua stack.
		template<typename... T> bool callback(T... args) {
			bool any = L.callback(callbacks, args...);
			if(fn_cbname != "" && L.callback(fn_cbname, args...))
				any = true;
			return any;
		}
		const std::string& get_name() { return name; }
		void clear() { callbacks.clear(); }
	private:
		lua_callback_list(const lua_callback_list&);
		lua_callback_list& operator=(const lua_callback_list&);
		std::list<char> callbacks;
		lua_state& L;
		std::string name;
		std::string fn_cbname;
	};
/**
 * Enumerate all callbacks.
 */
	std::list<lua_callback_list*> get_callbacks()
	{
		if(master)
			return master->get_callbacks();
		std::list<lua_callback_list*> r;
		for(auto i : callbacks)
			r.push_back(i.second);
		return r;
	}
/**
 * Register a callback.
 */
	class callback_proxy
	{
	public:
		callback_proxy(lua_state& _L) : parent(_L) {}
		void do_register(const std::string& name, lua_callback_list& callback)
		{
			parent.do_register_cb(name, callback);
		}
/**
 * Unregister a callback.
 */
		void do_unregister(const std::string& name)
		{
			parent.do_unregister_cb(name);
		}
	private:
		lua_state& parent;
	};

	void do_register_cb(const std::string& name, lua_callback_list& callback)
	{
		callbacks[name] = &callback;
	}

	void do_unregister_cb(const std::string& name)
	{
		callbacks.erase(name);
	}

	//All kinds of Lua API functions.
	void pop(int n) { lua_pop(lua_handle, n); }
	void* newuserdata(size_t size) { return lua_newuserdata(lua_handle, size); }
	int setmetatable(int index) { return lua_setmetatable(lua_handle, index); }
	int type(int index) { return lua_type(lua_handle, index); }
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
	void pushnumber(lua_Number n) { return lua_pushnumber(lua_handle, n); }
	int isnumber(int index) { return lua_isnumber(lua_handle, index); }
	int isboolean(int index) { return lua_isboolean(lua_handle, index); }
	int toboolean(int index) { return lua_toboolean(lua_handle, index); }
	const char* tolstring(int index, size_t *len) { return lua_tolstring(lua_handle, index, len); }
	void pushboolean(int b) { lua_pushboolean(lua_handle, b); }
	lua_Number tonumber(int index) { return lua_tonumber(lua_handle, index); }
	void gettable(int index) { lua_gettable(lua_handle, index); }
#if LUA_VERSION_NUM == 501
	int load(lua_Reader reader, void* data, const char* chunkname) { return lua_load(lua_handle, reader, data,
		chunkname); }
#endif
#if LUA_VERSION_NUM == 502
	int load(lua_Reader reader, void* data, const char* chunkname, const char* mode) { return lua_load(lua_handle,
		reader, data, chunkname, mode); }
#endif
	const char* tostring(int index) { return lua_tostring(lua_handle, index); }
	const char* tolstring(int index, size_t& len) { return lua_tolstring(lua_handle, index, &len); }
	void pushlstring(const char* s, size_t len) { lua_pushlstring(lua_handle, s, len); }
	void pushlstring(const std::string& s) { lua_pushlstring(lua_handle, s.c_str(), s.length()); }
	void pushlstring(const char32_t* s, size_t len) { pushlstring(to_u8string(std::u32string(s, len))); }
	int pcall(int nargs, int nresults, int errfunc) { return lua_pcall(lua_handle, nargs, nresults, errfunc); }
	int next(int index) { return lua_next(lua_handle, index); }
	int isnoneornil(int index) { return lua_isnoneornil(lua_handle, index); }
	lua_Integer tointeger(int index) { return lua_tointeger(lua_handle, index); }
	void rawgeti(int index, int n) { lua_rawgeti(lua_handle, index, n); }
	callback_proxy cbproxy;
private:
	static void builtin_oom();
	static void* builtin_alloc(void* user, void* old, size_t olds, size_t news);
	void (*oom_handler)();
	lua_state* master;
	lua_State* lua_handle;
	std::set<std::pair<lua_function_group*, int>> function_groups;
	std::map<std::string, lua_callback_list*> callbacks;
	lua_state(lua_state&);
	lua_state& operator=(lua_state&);
};

/**
 * Pin of an object against Lua GC.
 */
template<typename T> struct lua_obj_pin
{
/**
 * Create a null pin.
 */
	lua_obj_pin()
	{
		state = NULL;
		obj = NULL;
	}
/**
 * Create a new pin.
 *
 * Parameter _state: The state to pin the object in.
 * Parameter _object: The object to pin.
 */
	lua_obj_pin(lua_state& _state, T* _object)
		: state(&_state.get_master())
	{
		_state.pushlightuserdata(this);
		_state.pushvalue(-2);
		_state.rawset(LUA_REGISTRYINDEX);
		obj = _object;
	}
/**
 * Delete a pin.
 */
	~lua_obj_pin() throw()
	{
		if(obj) {
			state->pushlightuserdata(this);
			state->pushnil();
			state->rawset(LUA_REGISTRYINDEX);
		}
	}
/**
 * Copy ctor.
 */
	lua_obj_pin(const lua_obj_pin& p)
	{
		state = p.state;
		obj = p.obj;
		if(obj) {
			state->pushlightuserdata(this);
			state->pushlightuserdata(const_cast<lua_obj_pin*>(&p));
			state->rawget(LUA_REGISTRYINDEX);
			state->rawset(LUA_REGISTRYINDEX);
		}
	}
/**
 * Assignment operator.
 */
	lua_obj_pin& operator=(const lua_obj_pin& p)
	{
		if(state == p.state && obj == p.obj)
			return *this;
		if(obj == p.obj)
			throw std::logic_error("A Lua object can't be in two lua states at once");
		if(obj) {
			state->pushlightuserdata(this);
			state->pushnil();
			state->rawset(LUA_REGISTRYINDEX);
		}
		state = p.state;
		if(p.obj) {
			state->pushlightuserdata(this);
			state->pushlightuserdata(const_cast<lua_obj_pin*>(&p));
			state->rawget(LUA_REGISTRYINDEX);
			state->rawset(LUA_REGISTRYINDEX);
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
			state->pushlightuserdata(this);
			state->pushnil();
			state->rawset(LUA_REGISTRYINDEX);
		}
		state = NULL;
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
	void luapush(lua_state& state)
	{
		state.pushlightuserdata(this);
		state.rawget(LUA_REGISTRYINDEX);
	}
private:
	T* obj;
	lua_state* state;
};

template<typename T> void* unbox_any_pin(struct lua_obj_pin<T>* pin)
{
	return pin ? pin->object() : NULL;
}

/**
 * Helper class containing binding data for Lua class call.
 */
template<class T> struct lua_class_bind_data
{
/**
 * The pointer to call.
 */
	int (T::*fn)(lua_state& state, const std::string& _fname);
/**
 * The state to call it in.
 */
	lua_state* state;
/**
 * The name of the method to pass.
 */
	char fname[];
};

template<class T> class lua_class;

/**
 * Function to obtain class object for given Lua class.
 */
template<class T> lua_class<T>& objclass();

template<class T> struct lua_class_binding
{
/**
 * Name.
 */
	const char* name;
/**
 * Function.
 */
	int (T::*fn)(lua_state& LS, const std::string& fname);
};

/**
 * The type of Lua classes.
 */
template<class T> class lua_class
{
	template<typename... U> T* _create(lua_state& state, U... args)
	{
		void* obj = state.newuserdata(sizeof(T));
		load_metatable(state);
		state.setmetatable(-2);
		T* _obj = reinterpret_cast<T*>(obj);
		new(_obj) T(state, args...);
		return _obj;
	}

	static int class_bind_trampoline(lua_State* LS)
	{
		try {
			lua_class_bind_data<T>* b = (lua_class_bind_data<T>*)lua_touserdata(LS, lua_upvalueindex(1));
			lua_state L(*b->state, LS);
			T* p = lua_class<T>::get(L, 1, b->fname);
			return (p->*(b->fn))(L, b->fname);
		} catch(std::exception& e) {
			std::string err = e.what();
			lua_pushlstring(LS, err.c_str(), err.length());
			lua_error(LS);
		}
	}

	T* _get(lua_state& state, int arg, const std::string& fname, bool optional = false)
	{
		if(state.type(arg) == LUA_TNONE || state.type(arg) == LUA_TNIL) {
			if(optional)
				return NULL;
			else
				goto badtype;
		}
		load_metatable(state);
		if(!state.getmetatable(arg))
			goto badtype;
		if(!state.rawequal(-1, -2))
			goto badtype;
		state.pop(2);
		return reinterpret_cast<T*>(state.touserdata(arg));
badtype:
		(stringfmt() << "argument #" << arg << " to " << fname << " must be " << name).throwex();
		return NULL;	//Never reached.
	}

	bool _is(lua_state& state, int arg)
	{
		if(state.type(arg) != LUA_TUSERDATA)
			return false;
		load_metatable(state);
		if(!state.getmetatable(arg)) {
			state.pop(1);
			return false;
		}
		bool ret = state.rawequal(-1, -2);
		state.pop(2);
		return ret;
	}

	lua_obj_pin<T> _pin(lua_state& state, int arg, const std::string& fname)
	{
		T* obj = get(state, arg, fname);
		state.pushvalue(arg);
		return lua_obj_pin<T>(state, obj);
	}
public:
/**
 * Create a new Lua class.
 *
 * Parameter _name: The name of the class.
 */
	lua_class(const std::string& _name)
	{
		name = _name;
		luaclass_methods m;
		m.is = lua_class<T>::is;
		m.name = lua_class<T>::get_name;
		m.print = lua_class<T>::print;
		userdata_recogn_fns().push_back(m);
	}

/**
 * Create a new instance of object.
 *
 * Parameter state: The Lua state to create the object in.
 * Parameter args: The arguments to pass to class constructor.
 */
	template<typename... U> static T* create(lua_state& state, U... args)
	{
		return objclass<T>()._create(state, args...);
	}

/**
 * Bind a method to class.
 *
 * Parameter state: The state to do the binding in.
 * Parameter keyname: The name of the method.
 * Parameter fn: The method to call.
 * Parameter force: If true, overwrite existing method.
 */
	void bind(lua_state& state, const char* keyname, int (T::*fn)(lua_state& LS, const std::string& fname))
	{
		load_metatable(state);
		state.pushstring(keyname);
		std::string fname = name + std::string("::") + keyname;
		void* ptr = state.newuserdata(sizeof(lua_class_bind_data<T>) + fname.length() + 1);
		lua_class_bind_data<T>* bdata = reinterpret_cast<lua_class_bind_data<T>*>(ptr);
		bdata->fn = fn;
		bdata->state = &state.get_master();
		std::copy(fname.begin(), fname.end(), bdata->fname);
		bdata->fname[fname.length()] = 0;
		state.pushcclosure(class_bind_trampoline, 1);
		state.rawset(-3);
		state.pop(1);
	}
/**
 * Bind multiple at once.
 */
	void bind_multi(lua_state& state, std::initializer_list<lua_class_binding<T>> list, void* doonce_key = NULL)
	{
		static char once_key;
		if(state.do_once(doonce_key ? doonce_key : &once_key))
			for(auto i : list)
				bind(state, i.name, i.fn);
	}
/**
 * Get a pointer to the object.
 *
 * Parameter state: The Lua state.
 * Parameter arg: Argument index.
 * Parameter fname: The name of function for error messages.
 * Parameter optional: If true and argument is NIL or none, return NULL.
 * Throws std::runtime_error: Wrong type.
 */
	static T* get(lua_state& state, int arg, const std::string& fname, bool optional = false)
		throw(std::bad_alloc, std::runtime_error)
	{
		return objclass<T>()._get(state, arg, fname, optional);
	}

/**
 * Identify if object is of this type.
 *
 * Parameter state: The Lua state.
 * Parameter arg: Argument index.
 * Returns: True if object is of specified type, false if not.
 */
	static bool is(lua_state& state, int arg) throw()
	{
		return objclass<T>()._is(state, arg);
	}
/**
 * Get name of class.
 */
	static const std::string& get_name()
	{
		return objclass<T>().name;
	}
/**
 * Format instance of this class as string.
 */
	static std::string print(lua_state& state, int index)
	{
		T* obj = get(state, index, "__internal_print");
		return obj->print();
	}
/**
 * Get a pin of object against Lua GC.
 *
 * Parameter state: The Lua state.
 * Parameter arg: Argument index.
 * Parameter fname: Name of function for error message purposes.
 * Throws std::runtime_error: Wrong type.
 */
	static lua_obj_pin<T> pin(lua_state& state, int arg, const std::string& fname) throw(std::bad_alloc,
		std::runtime_error)
	{
		return objclass<T>()._pin(state, arg, fname);
	}
private:
	static int dogc(lua_State* LS)
	{
		T* obj = reinterpret_cast<T*>(lua_touserdata(LS, 1));
		obj->~T();
		return 0;
	}

	static int newindex(lua_State* LS)
	{
		lua_pushstring(LS, "Writing metatables of classes is not allowed");
		lua_error(LS);
		return 0;
	}

	static int index(lua_State* LS)
	{
		lua_getmetatable(LS, 1);
		lua_pushvalue(LS, 2);
		lua_rawget(LS, -2);
		return 1;
	}

	void load_metatable(lua_state& state)
	{
again:
		state.pushlightuserdata(this);
		state.rawget(LUA_REGISTRYINDEX);
		if(state.type(-1) == LUA_TNIL) {
			state.pop(1);
			state.pushlightuserdata(this);
			state.newtable();
			state.pushvalue(-1);
			state.setmetatable(-2);
			state.pushstring("__gc");
			state.pushcfunction(&lua_class<T>::dogc);
			state.rawset(-3);
			state.pushstring("__newindex");
			state.pushcfunction(&lua_class<T>::newindex);
			state.rawset(-3);
			state.pushstring("__index");
			state.pushcfunction(&lua_class<T>::index);
			state.rawset(-3);
			state.rawset(LUA_REGISTRYINDEX);
			goto again;
		}
	}
	std::string name;
	lua_class(const lua_class<T>&);
	lua_class& operator=(const lua_class<T>&);
};

#define DECLARE_LUACLASS(x, X) template<> lua_class< x >& objclass() { static lua_class< x > clazz( X ); \
	return clazz; }

/**
 * Function implemented in C++ exported to Lua.
 */
class lua_function
{
public:
/**
 * Register function.
 */
	lua_function(lua_function_group& group, const std::string& name) throw(std::bad_alloc);
/**
 * Unregister function.
 */
	virtual ~lua_function() throw();

/**
 * Invoke function.
 */
	virtual int invoke(lua_state& L) = 0;
protected:
	std::string fname;
	lua_function_group& group;
};

/**
 * Register function pointer as lua function.
 */
class function_ptr_luafun : public lua_function
{
public:
/**
 * Register.
 */
	function_ptr_luafun(lua_function_group& group, const std::string& name,
		int (*_fn)(lua_state& L, const std::string& fname))
		: lua_function(group, name)
	{
		fn = _fn;
	}
/**
 * Invoke function.
 */
	int invoke(lua_state& L)
	{
		return fn(L, fname);
	}
private:
	int (*fn)(lua_state& L, const std::string& fname);
};


#endif
