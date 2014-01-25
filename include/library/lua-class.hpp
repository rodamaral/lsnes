#ifndef _library__lua_class__hpp__included__
#define _library__lua_class__hpp__included__

#include "lua-base.hpp"
#include "lua-pin.hpp"

namespace lua
{
struct class_ops
{
	bool (*is)(state& _state, int index);
	const std::string& (*name)();
	std::string (*print)(state& _state, int index);
};

std::list<class_ops>& userdata_recogn_fns();
std::string try_recognize_userdata(state& _state, int index);
std::string try_print_userdata(state& _state, int index);
std::unordered_map<std::type_index, void*>& class_types();

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
		objpin<T> t(_state, obj);
		_state.pop(1);
		return t;
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
}

#endif
