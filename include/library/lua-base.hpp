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
extern "C"
{
#include <lua.h>
}

namespace lua
{
class state;

struct class_ops
{
	bool (*is)(state& _state, int index);
	const std::string& (*name)();
	std::string (*print)(state& _state, int index);
};

std::list<class_ops>& userdata_recogn_fns();
std::string try_recognize_userdata(state& _state, int index);
std::string try_print_userdata(state& _state, int index);

struct function;

std::unordered_map<std::type_index, void*>& class_types();

/**
 * Group of functions.
 */
class function_group
{
public:
/**
 * Create a group.
 */
	function_group();
/**
 * Destroy a group.
 */
	~function_group();
/**
 * Add a function to group.
 */
	void do_register(const std::string& name, function& fun);
/**
 * Drop a function from group.
 */
	void do_unregister(const std::string& name);
/**
 * Request callbacks on all currently registered functions.
 */
	void request_callback(std::function<void(std::string, function*)> cb);
/**
 * Bind a callback.
 *
 * Callbacks for all registered functions are immediately called.
 */
	int add_callback(std::function<void(std::string, function*)> cb,
		std::function<void(function_group*)> dcb);
/**
 * Unbind a calback.
 */
	void drop_callback(int handle);
private:
	int next_handle;
	std::map<std::string, function*> functions;
	std::map<int, std::function<void(std::string, function*)>> callbacks;
	std::map<int, std::function<void(function_group*)>> dcallbacks;
};

/**
 * Lua state object.
 */
class state
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
			(stringfmt() << "Argument #" << argindex << " to " << fname << " must be numeric").throwex();
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
			(stringfmt() << "Argument #" << argindex << " to " << fname << " must be numeric if "
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
	void add_function_group(function_group& group);
/**
 * Function callback.
 */
	void function_callback(const std::string& name, function* func);
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
	std::list<callback_list*> get_callbacks()
	{
		if(master)
			return master->get_callbacks();
		std::list<callback_list*> r;
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
		callback_proxy(state& _L) : parent(_L) {}
		void do_register(const std::string& name, callback_list& callback)
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
		state& parent;
	};

	void do_register_cb(const std::string& name, callback_list& callback)
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
	void pushlstring(const char32_t* s, size_t len) { pushlstring(utf8::to8(std::u32string(s, len))); }
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
	state* master;
	lua_State* lua_handle;
	std::set<std::pair<function_group*, int>> function_groups;
	std::map<std::string, callback_list*> callbacks;
	state(state&);
	state& operator=(state&);
};

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

/**
 * Helper class containing binding data for Lua class call.
 */
template<class T> struct class_binding
{
/**
 * The pointer to call.
 */
	int (T::*fn)(state& lstate, const std::string& _fname);
/**
 * The state to call it in.
 */
	state* _state;
/**
 * The name of the method to pass.
 */
	char fname[];
};

template<class T> class _class;

/**
 * Function to obtain class object for given Lua class.
 */
template<class T> _class<T>& objclass()
{
	auto& type = typeid(T);
	if(!class_types().count(type))
		throw std::runtime_error("Internal error: Lua class not found!");
	return *reinterpret_cast<_class<T>*>(class_types()[type]);
}

template<class T> struct class_method
{
/**
 * Name.
 */
	const char* name;
/**
 * Function.
 */
	int (T::*fn)(state& LS, const std::string& fname);
};

/**
 * The type of Lua classes.
 */
template<class T> class _class
{
	template<typename... U> T* _create(state& _state, U... args)
	{
		void* obj = _state.newuserdata(sizeof(T));
		load_metatable(_state);
		_state.setmetatable(-2);
		T* _obj = reinterpret_cast<T*>(obj);
		new(_obj) T(_state, args...);
		return _obj;
	}

	static int class_bind_trampoline(lua_State* LS)
	{
		try {
			class_binding<T>* b = (class_binding<T>*)lua_touserdata(LS, lua_upvalueindex(1));
			state L(*b->_state, LS);
			T* p = _class<T>::get(L, 1, b->fname);
			return (p->*(b->fn))(L, b->fname);
		} catch(std::exception& e) {
			std::string err = e.what();
			lua_pushlstring(LS, err.c_str(), err.length());
			lua_error(LS);
		}
	}

	T* _get(state& _state, int arg, const std::string& fname, bool optional = false)
	{
		if(_state.type(arg) == LUA_TNONE || _state.type(arg) == LUA_TNIL) {
			if(optional)
				return NULL;
			else
				goto badtype;
		}
		load_metatable(_state);
		if(!_state.getmetatable(arg))
			goto badtype;
		if(!_state.rawequal(-1, -2))
			goto badtype;
		_state.pop(2);
		return reinterpret_cast<T*>(_state.touserdata(arg));
badtype:
		(stringfmt() << "argument #" << arg << " to " << fname << " must be " << name).throwex();
		return NULL;	//Never reached.
	}

	bool _is(state& _state, int arg)
	{
		if(_state.type(arg) != LUA_TUSERDATA)
			return false;
		load_metatable(_state);
		if(!_state.getmetatable(arg)) {
			_state.pop(1);
			return false;
		}
		bool ret = _state.rawequal(-1, -2);
		_state.pop(2);
		return ret;
	}

	objpin<T> _pin(state& _state, int arg, const std::string& fname)
	{
		T* obj = get(_state, arg, fname);
		_state.pushvalue(arg);
		return objpin<T>(_state, obj);
	}
public:
/**
 * Create a new Lua class.
 *
 * Parameter _name: The name of the class.
 */
	_class(const std::string& _name)
	{
		name = _name;
		class_ops m;
		m.is = _class<T>::is;
		m.name = _class<T>::get_name;
		m.print = _class<T>::print;
		userdata_recogn_fns().push_back(m);
		auto& type = typeid(T);
		class_types()[type] = this;
	}

/**
 * Create a new instance of object.
 *
 * Parameter _state: The Lua state to create the object in.
 * Parameter args: The arguments to pass to class constructor.
 */
	template<typename... U> static T* create(state& _state, U... args)
	{
		return objclass<T>()._create(_state, args...);
	}

/**
 * Bind a method to class.
 *
 * Parameter _state: The state to do the binding in.
 * Parameter keyname: The name of the method.
 * Parameter fn: The method to call.
 * Parameter force: If true, overwrite existing method.
 */
	void bind(state& _state, const char* keyname, int (T::*fn)(state& LS, const std::string& fname))
	{
		load_metatable(_state);
		_state.pushstring(keyname);
		std::string fname = name + std::string("::") + keyname;
		void* ptr = _state.newuserdata(sizeof(class_binding<T>) + fname.length() + 1);
		class_binding<T>* bdata = reinterpret_cast<class_binding<T>*>(ptr);
		bdata->fn = fn;
		bdata->_state = &_state.get_master();
		std::copy(fname.begin(), fname.end(), bdata->fname);
		bdata->fname[fname.length()] = 0;
		_state.pushcclosure(class_bind_trampoline, 1);
		_state.rawset(-3);
		_state.pop(1);
	}
/**
 * Bind multiple at once.
 */
	void bind_multi(state& _state, std::initializer_list<class_method<T>> list, void* doonce_key = NULL)
	{
		static char once_key;
		if(_state.do_once(doonce_key ? doonce_key : &once_key))
			for(auto i : list)
				bind(_state, i.name, i.fn);
	}
/**
 * Get a pointer to the object.
 *
 * Parameter _state: The Lua state.
 * Parameter arg: Argument index.
 * Parameter fname: The name of function for error messages.
 * Parameter optional: If true and argument is NIL or none, return NULL.
 * Throws std::runtime_error: Wrong type.
 */
	static T* get(state& _state, int arg, const std::string& fname, bool optional = false)
		throw(std::bad_alloc, std::runtime_error)
	{
		return objclass<T>()._get(_state, arg, fname, optional);
	}

/**
 * Identify if object is of this type.
 *
 * Parameter _state: The Lua state.
 * Parameter arg: Argument index.
 * Returns: True if object is of specified type, false if not.
 */
	static bool is(state& _state, int arg) throw()
	{
		try {
			return objclass<T>()._is(_state, arg);
		} catch(...) {
			return false;
		}
	}
/**
 * Get name of class.
 */
	static const std::string& get_name()
	{
		try {
			return objclass<T>().name;
		} catch(...) {
			static std::string foo = "???";
			return foo;
		}
	}
/**
 * Format instance of this class as string.
 */
	static std::string print(state& _state, int index)
	{
		T* obj = get(_state, index, "__internal_print");
		return obj->print();
	}
/**
 * Get a pin of object against Lua GC.
 *
 * Parameter _state: The Lua state.
 * Parameter arg: Argument index.
 * Parameter fname: Name of function for error message purposes.
 * Throws std::runtime_error: Wrong type.
 */
	static objpin<T> pin(state& _state, int arg, const std::string& fname) throw(std::bad_alloc,
		std::runtime_error)
	{
		return objclass<T>()._pin(_state, arg, fname);
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

	void load_metatable(state& _state)
	{
again:
		_state.pushlightuserdata(this);
		_state.rawget(LUA_REGISTRYINDEX);
		if(_state.type(-1) == LUA_TNIL) {
			_state.pop(1);
			_state.pushlightuserdata(this);
			_state.newtable();
			_state.pushvalue(-1);
			_state.setmetatable(-2);
			_state.pushstring("__gc");
			_state.pushcfunction(&_class<T>::dogc);
			_state.rawset(-3);
			_state.pushstring("__newindex");
			_state.pushcfunction(&_class<T>::newindex);
			_state.rawset(-3);
			_state.pushstring("__index");
			_state.pushcfunction(&_class<T>::index);
			_state.rawset(-3);
			_state.rawset(LUA_REGISTRYINDEX);
			goto again;
		}
	}
	std::string name;
	_class(const _class<T>&);
	_class& operator=(const _class<T>&);
};

/**
 * Function implemented in C++ exported to Lua.
 */
class function
{
public:
/**
 * Register function.
 */
	function(function_group& group, const std::string& name) throw(std::bad_alloc);
/**
 * Unregister function.
 */
	virtual ~function() throw();

/**
 * Invoke function.
 */
	virtual int invoke(state& L) = 0;
protected:
	std::string fname;
	function_group& group;
};

/**
 * Register function pointer as lua function.
 */
class fnptr : public function
{
public:
/**
 * Register.
 */
	fnptr(function_group& group, const std::string& name,
		int (*_fn)(state& L, const std::string& fname))
		: function(group, name)
	{
		fn = _fn;
	}
/**
 * Invoke function.
 */
	int invoke(state& L)
	{
		return fn(L, fname);
	}
private:
	int (*fn)(state& L, const std::string& fname);
};
}

#endif
