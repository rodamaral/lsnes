#ifndef _library__lua_class__hpp__included__
#define _library__lua_class__hpp__included__

#include "lua-base.hpp"
#include "lua-pin.hpp"

namespace lua
{
class class_base;
class parameters;

/**
 * Group of classes.
 */
class class_group
{
public:
/**
 * Create a group.
 */
	class_group();
/**
 * Destroy a group.
 */
	~class_group();
/**
 * Add a class to group.
 */
	void do_register(const std::string& name, class_base& fun);
/**
 * Drop a class from group.
 */
	void do_unregister(const std::string& name, class_base* dummy);
/**
 * Request callbacks on all currently registered functions.
 */
	void request_callback(std::function<void(std::string, class_base*)> cb);
/**
 * Bind a callback.
 *
 * Callbacks for all registered functions are immediately called.
 */
	int add_callback(std::function<void(std::string, class_base*)> cb,
		std::function<void(class_group*)> dcb);
/**
 * Unbind a calback.
 */
	void drop_callback(int handle);
private:
	int next_handle;
	std::map<std::string, class_base*> classes;
	std::map<int, std::function<void(std::string, class_base*)>> callbacks;
	std::map<int, std::function<void(class_group*)>> dcallbacks;
};

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
	int (T::*fn)(state& lstate, lua::parameters& P);
/**
 * The state to call it in.
 */
	state* _state;
/**
 * The name of the method to pass.
 */
	char fname[];
};

/**
 * Helper class containing binding data for Lua static class call.
 */
struct static_binding
{
/**
 * The pointer to call.
 */
	int (*fn)(state& lstate, parameters& P);
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

/**
 * A class method.
 */
template<class T> struct class_method
{
/**
 * Name.
 */
	const char* name;
/**
 * Function.
 */
	int (T::*fn)(state& LS, lua::parameters& P);
};

/**
 * A static class method.
 */
struct static_method
{
/**
 * Name.
 */
	const char* name;
/**
 * Function.
 */
	int (*fn)(state& LS, parameters& P);
};

/**
 * Virtual base of Lua classes
 */
class class_base
{
public:
/**
 * Create a new Lua class.
 *
 * Parameter _group: The group the class will be in.
 * Parameter _name: The name of the class.
 */
	class_base(class_group& _group, const std::string& _name);
/**
 * Dtor.
 */
	virtual ~class_base() throw();
/**
 * Lookup by name in given Lua state.
 *
 * Parameter _L: The Lua state to look in.
 * Parameter _name: The name of the class.
 * Returns: The class instance, or NULL if no match.
 */
	static class_base* lookup(state& L, const std::string& _name);
/**
 * Push class table to stack.
 */
	static bool lookup_and_push(state& L, const std::string& _name);
/**
 * Get set of all classes.
 */
	static std::set<std::string> all_classes(state& L);
/**
 * Register in given Lua state.
 */
	virtual void register_state(state& L) = 0;
/**
 * Lookup static methods in class.
 */
	virtual std::list<static_method> static_methods() = 0;
/**
 * Lookup class methods in class.
 */
	virtual std::set<std::string> class_methods() = 0;
/**
 * Get name of class.
 */
	const std::string& get_name() { return name; }
protected:
	void delayed_register();
	void register_static(state& L);
private:
	class_group& group;
	std::string name;
	bool registered;
};

static const size_t overcommit_std_align = 32;

/**
 * Align a overcommit pointer.
 */
template<typename T, typename U> U* align_overcommit(T* th)
{
	size_t ptr = reinterpret_cast<size_t>(th) + sizeof(T);
	return reinterpret_cast<U*>(ptr + (overcommit_std_align - ptr % overcommit_std_align) % overcommit_std_align);
}

/**
 * The type of Lua classes.
 */
template<class T> class _class : public class_base
{
	template<typename... U> T* _create(state& _state, U... args)
	{
		size_t overcommit = T::overcommit(args...);
		void* obj = _state.newuserdata(sizeof(T) + overcommit);
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
			lua::parameters P(L, b->fname);
			return (p->*(b->fn))(L, P);
		} catch(std::exception& e) {
			std::string err = e.what();
			lua_pushlstring(LS, err.c_str(), err.length());
			lua_error(LS);
		}
		return 0; //NOTREACHED
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

	void bind(state& _state, const char* keyname, int (T::*fn)(state& LS, lua::parameters& P))
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
protected:
	void register_state(state& L)
	{
		static char once_key;
		register_static(L);
		if(L.do_once(&once_key))
			for(auto i : cmethods)
				bind(L, i.name, i.fn);
	}
public:
/**
 * Create a new Lua class.
 *
 * Parameter _group: The group the class will be in.
 * Parameter _name: The name of the class.
 * Parameter _smethods: Static methods of the class.
 * Parameter _cmethods: Class methods of the class.
 * Parameter _print: The print method.
 */
	_class(class_group& _group, const std::string& _name, std::initializer_list<static_method> _smethods,
		std::initializer_list<class_method<T>> _cmethods = {}, std::string (T::*_print)() = NULL)
		: class_base(_group, _name), smethods(_smethods), cmethods(_cmethods)
	{
		name = _name;
		class_ops m;
		printmeth = _print;
		m.is = _class<T>::is;
		m.name = _class<T>::get_name;
		m.print = _class<T>::print;
		userdata_recogn_fns().push_back(m);
		auto& type = typeid(T);
		class_types()[type] = this;
		delayed_register();
	}
/**
 * Dtor
 */
	~_class() throw()
	{
		auto& type = typeid(T);
		class_types().erase(type);
		auto& fns = userdata_recogn_fns();
		for(auto i = fns.begin(); i != fns.end(); i++) {
			if(i->is == _class<T>::is) {
				fns.erase(i);
				break;
			}
		}
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
		try {
			auto pmeth = objclass<T>().printmeth;
			if(pmeth)
				return (obj->*pmeth)();
			else
				return "";
		} catch(...) {
			return "";
		}
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
/**
 * Lookup static methods.
 */
	std::list<static_method> static_methods()
	{
		return smethods;
	}
/**
 * Lookup class methods.
 */
	std::set<std::string> class_methods()
	{
		std::set<std::string> r;
		for(auto& i : cmethods)
			r.insert(i.name);
		return r;
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
		if(lua_type(LS, -1) == LUA_TNIL) {
			std::string err = std::string("Class '") + lua_tostring(LS, lua_upvalueindex(1)) +
				"' does not have class method '" + lua_tostring(LS, 2) + "'";
			lua_pushstring(LS, err.c_str());
			lua_error(LS);
		}
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
			_state.pushlstring(name);
			_state.pushcclosure(&_class<T>::index, 1);
			_state.rawset(-3);
			_state.rawset(LUA_REGISTRYINDEX);
			goto again;
		}
	}
	std::string name;
	std::list<static_method> smethods;
	std::list<class_method<T>> cmethods;
	std::string (T::*printmeth)();
	_class(const _class<T>&);
	_class& operator=(const _class<T>&);
};
}

#endif
