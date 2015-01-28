#include "lua-base.hpp"
#include "lua-class.hpp"
#include "lua-function.hpp"
#include "lua-params.hpp"
#include "lua-pin.hpp"
#include "stateobject.hpp"
#include "threads.hpp"
#include <iostream>
#include <cassert>

namespace lua
{
namespace
{
	threads::rlock* global_lock;
	threads::rlock& get_lua_lock()
	{
		if(!global_lock) global_lock = new threads::rlock;
		return *global_lock;
	}

	struct state_internal
	{
		std::map<std::string, state::callback_list*> callbacks;
		std::set<std::pair<function_group*, int>> function_groups;
		std::set<std::pair<class_group*, int>> class_groups;
	};

	struct fgroup_internal
	{
		fgroup_internal()
		{
			next_handle = 0;
		}
		std::map<std::string, function*> functions;
		int next_handle;
		std::map<int, std::function<void(std::string, function*)>> callbacks;
		std::map<int, std::function<void(function_group*)>> dcallbacks;
	};

	struct cgroup_internal
	{
		cgroup_internal()
		{
			next_handle = 0;
		}
		int next_handle;
		std::map<std::string, class_base*> classes;
		std::map<int, std::function<void(std::string, class_base*)>> callbacks;
		std::map<int, std::function<void(class_group*)>> dcallbacks;
	};

	typedef stateobject::type<state, state_internal> state_internal_t;
	typedef stateobject::type<function_group, fgroup_internal> fgroup_internal_t;
	typedef stateobject::type<class_group, cgroup_internal> cgroup_internal_t;
}

std::unordered_map<std::type_index, void*>& class_types()
{
	static std::unordered_map<std::type_index, void*> x;
	return x;
}
namespace
{
	char classtable_key;
	char classtable_meta_key;

	struct class_info
	{
		class_base* obj;
		static int index(lua_State* L);
		static int newindex(lua_State* L);
		static int pairs(lua_State* L);
		static int pairs_next(lua_State* L);
		static int smethods(lua_State* L);
		static int cmethods(lua_State* L);
		static int trampoline(lua_State* L);
		static void check(lua_State* L);
	};

	void class_info::check(lua_State* L)
	{
		lua_pushlightuserdata(L, &classtable_meta_key);
		lua_rawget(L, LUA_REGISTRYINDEX);
		lua_getmetatable(L, 1);
		if(!lua_rawequal(L, -1, -2)) {
			lua_pushstring(L, "Bad class table");
			lua_error(L);
		}
		lua_pop(L, 2);
	}

	int class_info::index(lua_State* L)
	{
		check(L);

		class_base* ptr = ((class_info*)lua_touserdata(L, 1))->obj;
		const char* method = lua_tostring(L, 2);
		if(!method) {
			lua_pushstring(L, "Indexing invalid element of class table");
			lua_error(L);
		}
		if(!strcmp(method, "_static_methods")) {
			lua_pushlightuserdata(L, ptr);
			lua_pushcclosure(L, class_info::smethods, 1);
			return 1;
		} else if(!strcmp(method, "_class_methods")) {
			lua_pushlightuserdata(L, ptr);
			lua_pushcclosure(L, class_info::cmethods, 1);
			return 1;
		} else {
			auto m = ptr->static_methods();
			for(auto i : m) {
				if(!strcmp(i.name, method)) {
					//Hit.
					std::string name = ptr->get_name() + "::" + method;
					lua_pushvalue(L, lua_upvalueindex(1));  //State.
					lua_pushlightuserdata(L, (void*)i.fn);
					std::string fname = ptr->get_name() + "::" + i.name;
					lua_pushstring(L, fname.c_str());
					lua_pushcclosure(L, class_info::trampoline, 3);
					return 1;
				}
			}
			std::string err = std::string("Class '") + ptr->get_name() +
				"' does not have static method '" + method + "'";
			lua_pushstring(L, err.c_str());
			lua_error(L);
		}
		return 0; //NOTREACHED
	}
	int class_info::newindex(lua_State* L)
	{
		lua_pushstring(L, "Writing into class table not allowed");
		lua_error(L);
		return 0; //NOTREACHED
	}

	int class_info::pairs(lua_State* _xL)
	{
		check(_xL);

		lua::state* _L = (lua::state*)lua_touserdata(_xL, lua_upvalueindex(1));
		state L(*_L, _xL);

		L.pushvalue(lua_upvalueindex(1));
		L.pushcclosure(class_info::pairs_next, 1);	//Next
		L.pushvalue(1);					//State.
		L.pushnil();					//Index.
		return 3;
	}

	int class_info::pairs_next(lua_State* _xL)
	{
		check(_xL);

		lua::state* _L = (lua::state*)lua_touserdata(_xL, lua_upvalueindex(1));
		state L(*_L, _xL);
		class_base* obj = ((class_info*)L.touserdata(1))->obj;
		auto m = obj->static_methods();
		std::string key = (L.type(2) == LUA_TSTRING) ? L.tostring(2) : "";
		std::string lowbound = "\xFF";	//Sorts greater than anything that is valid UTF-8.
		void* best_fn = NULL;
		for(auto i : m)
			if(lowbound > i.name && i.name > key) {
				lowbound = i.name;
				best_fn = (void*)i.fn;
			}
		if(best_fn) {
			L.pushlstring(lowbound);
			std::string name = obj->get_name() + "::" + lowbound;
			L.pushvalue(lua_upvalueindex(1));  //State.
			L.pushlightuserdata(best_fn);
			L.pushlstring(name);
			L.pushcclosure(class_info::trampoline, 3);
			return 2;
		} else {
			L.pushnil();
			return 1;
		}
	}

	int class_info::smethods(lua_State* L)
	{
		class_base* obj = (class_base*)lua_touserdata(L, lua_upvalueindex(1));
		auto m = obj->static_methods();
		int rets = 0;
		for(auto i : m) {
			lua_pushstring(L, i.name);
			rets++;
		}
		return rets;
	}

	int class_info::cmethods(lua_State* L)
	{
		class_base* obj = (class_base*)lua_touserdata(L, lua_upvalueindex(1));
		auto m = obj->class_methods();
		int rets = 0;
		for(auto i : m) {
			lua_pushstring(L, i.c_str());
			rets++;
		}
		return rets;
	}

	typedef int (*fn_t)(state& L, parameters& P);

	int class_info::trampoline(lua_State* L)
	{
		state* lstate = reinterpret_cast<state*>(lua_touserdata(L, lua_upvalueindex(1)));
		void* _fn = lua_touserdata(L, lua_upvalueindex(2));
		fn_t fn = (fn_t)_fn;
		try {
			std::string name = lua_tostring(L, lua_upvalueindex(3));
			state _L(*lstate, L);
			parameters P(_L, name);
			return fn(_L, P);
		} catch(std::exception& e) {
			lua_pushfstring(L, "%s", e.what());
			lua_error(L);
		}
		return 0;
	}

	int lua_trampoline_function(lua_State* L)
	{
		void* ptr = lua_touserdata(L, lua_upvalueindex(1));
		state* lstate = reinterpret_cast<state*>(lua_touserdata(L, lua_upvalueindex(2)));
		function* f = reinterpret_cast<function*>(ptr);
		try {
			state _L(*lstate, L);
			return f->invoke(_L);
		} catch(std::exception& e) {
			lua_pushfstring(L, "%s", e.what());
			lua_error(L);
		}
		return 0;
	}

	//Pushes given table to top of stack, creating if needed.
	void recursive_lookup_table(state& L, const std::string& tab)
	{
		if(tab == "") {
			L.pushglobals();
			assert(L.type(-1) == LUA_TTABLE);
			return;
		}
		std::string u = tab;
		size_t split = u.find_last_of(".");
		std::string u1;
		std::string u2 = u;
		if(split < u.length()) {
			u1 = u.substr(0, split);
			u2 = u.substr(split + 1);
		}
		recursive_lookup_table(L, u1);
		L.getfield(-1, u2.c_str());
		if(L.type(-1) != LUA_TTABLE) {
			//Not a table, create a table.
			L.pop(1);
			L.newtable();
			L.setfield(-2, u2.c_str());
			L.getfield(-1, u2.c_str());
		}
		//Get rid of previous table.
		L.insert(-2);
		L.pop(1);
	}

	void register_function(state& L, const std::string& name, function* fun)
	{
		std::string u = name;
		size_t split = u.find_last_of(".");
		std::string u1;
		std::string u2 = u;
		if(split < u.length()) {
			u1 = u.substr(0, split);
			u2 = u.substr(split + 1);
		}
		recursive_lookup_table(L, u1);
		if(!fun)
			L.pushnil();
		else {
			void* ptr = reinterpret_cast<void*>(fun);
			L.pushlightuserdata(ptr);
			L.pushlightuserdata(&L);
			L.pushcclosure(lua_trampoline_function, 2);
		}
		L.setfield(-2, u2.c_str());
		L.pop(1);
	}

	void register_class(state& L, const std::string& name, class_base* fun)
	{
		fun->register_state(L);
	}
}

state::state() throw(std::bad_alloc)
{
	master = NULL;
	lua_handle = NULL;
	oom_handler = builtin_oom;
}

state::state(state& _master, lua_State* L)
{
	master = &_master;
	lua_handle = L;
}

state::~state() throw()
{
	if(master)
		return;
	threads::arlock h(get_lua_lock());
	auto state = state_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->function_groups)
		i.first->drop_callback(i.second);
	for(auto i : state->class_groups)
		i.first->drop_callback(i.second);
	if(lua_handle)
		lua_close(lua_handle);
	state_internal_t::clear(this);
}

void state::builtin_oom()
{
	std::cerr << "PANIC: FATAL: Out of memory" << std::endl;
	exit(1);
}

void* state::builtin_alloc(void* user, void* old, size_t olds, size_t news)
{
	if(news) {
		void* m = realloc(old, news);
		if(!m)
			reinterpret_cast<state*>(user)->oom_handler();
		return m;
	} else
		free(old);
	return NULL;
}


function::function(function_group& _group, const std::string& func) throw(std::bad_alloc)
	: group(_group)
{
	group.do_register(fname = func, *this);
}

function::~function() throw()
{
	group.do_unregister(fname, *this);
}

class_base::class_base(class_group& _group, const std::string& _name)
	: group(_group), name(_name)
{
	registered = false;
}

class_base::~class_base() throw()
{
	if(registered)
		group.do_unregister(name, *this);
}

void state::reset() throw(std::bad_alloc, std::runtime_error)
{
	if(master)
		return master->reset();
	threads::arlock h(get_lua_lock());
	auto state = &state_internal_t::get(this);
	if(lua_handle) {
		lua_State* tmp = lua_newstate(state::builtin_alloc, this);
		if(!tmp)
			throw std::runtime_error("Can't re-initialize Lua interpretter");
		lua_close(lua_handle);
		for(auto& i : state->callbacks)
			i.second->clear();
		lua_handle = tmp;
	} else {
		//Initialize new.
		lua_handle = lua_newstate(state::builtin_alloc, this);
		if(!lua_handle)
			throw std::runtime_error("Can't initialize Lua interpretter");
	}
	for(auto i : state->function_groups)
		i.first->request_callback([this](std::string name, function* func) -> void {
			register_function(*this, name, func);
		});
	for(auto i : state->class_groups)
		i.first->request_callback([this](std::string name, class_base* clazz) -> void {
			register_class(*this, name, clazz);
		});
}

void state::deinit() throw()
{
	if(master)
		return master->deinit();
	if(lua_handle)
		lua_close(lua_handle);
	lua_handle = NULL;
}

void state::add_function_group(function_group& group)
{
	threads::arlock h(get_lua_lock());
	auto& state = state_internal_t::get(this);
	state.function_groups.insert(std::make_pair(&group, group.add_callback([this](const std::string& name,
		function* func) -> void {
		this->function_callback(name, func);
	}, [this](function_group* x) {
		threads::arlock h(get_lua_lock());
		auto state = state_internal_t::get_soft(this);
		if(!state) return;
		for(auto i = state->function_groups.begin(); i != state->function_groups.end();)
			if(i->first == x)
				i = state->function_groups.erase(i);
			else
				i++;
	})));
}

void state::add_class_group(class_group& group)
{
	threads::arlock h(get_lua_lock());
	auto& state = state_internal_t::get(this);
	state.class_groups.insert(std::make_pair(&group, group.add_callback([this](const std::string& name,
		class_base* clazz) -> void {
		this->class_callback(name, clazz);
	}, [this](class_group* x) {
		threads::arlock h(get_lua_lock());
		auto state = state_internal_t::get_soft(this);
		if(!state) return;
		for(auto i = state->class_groups.begin(); i != state->class_groups.end();)
			if(i->first == x)
				i = state->class_groups.erase(i);
			else
				i++;
	})));
}

void state::function_callback(const std::string& name, function* func)
{
	if(master)
		return master->function_callback(name, func);
	if(lua_handle)
		register_function(*this, name, func);
}

void state::class_callback(const std::string& name, class_base* clazz)
{
	if(master)
		return master->class_callback(name, clazz);
	if(lua_handle)
		register_class(*this, name, clazz);
}

bool state::do_once(void* key)
{
	if(master)
		return master->do_once(key);
	pushlightuserdata(key);
	rawget(LUA_REGISTRYINDEX);
	if(type(-1) == LUA_TNIL) {
		pop(1);
		pushlightuserdata(key);
		pushlightuserdata(key);
		rawset(LUA_REGISTRYINDEX);
		return true;
	} else {
		pop(1);
		return false;
	}
}

std::list<state::callback_list*> state::get_callbacks()
{
	if(master)
		return master->get_callbacks();
	threads::arlock h(get_lua_lock());
	auto state = state_internal_t::get_soft(this);
	std::list<callback_list*> r;
	if(state)
		for(auto i : state->callbacks)
			r.push_back(i.second);
	return r;
}

void state::do_register(const std::string& name, callback_list& callback)
{
	threads::arlock h(get_lua_lock());
	auto& state = state_internal_t::get(this);
	if(state.callbacks.count(name)) return;
	state.callbacks[name] = &callback;
}

void state::do_unregister(const std::string& name, callback_list& callback)
{
	threads::arlock h(get_lua_lock());
	auto state = state_internal_t::get_soft(this);
	if(state && state->callbacks.count(name) && state->callbacks[name] == &callback)
		state->callbacks.erase(name);
}

state::callback_list::callback_list(state& _L, const std::string& _name, const std::string& fncbname)
	: L(_L), name(_name), fn_cbname(fncbname)
{
	L.do_register(name, *this);
}

state::callback_list::~callback_list()
{
	L.do_unregister(name, *this);
	if(!L.handle())
		return;
	for(auto& i : callbacks) {
		L.pushlightuserdata(&i);
		L.pushnil();
		L.rawset(LUA_REGISTRYINDEX);
	}
}

void state::callback_list::_register(state& _L)
{
	callbacks.push_back(0);
	_L.pushlightuserdata(&*callbacks.rbegin());
	_L.pushvalue(-2);
	_L.rawset(LUA_REGISTRYINDEX);
}

void state::callback_list::_unregister(state& _L)
{
	for(auto i = callbacks.begin(); i != callbacks.end();) {
		_L.pushlightuserdata(&*i);
		_L.rawget(LUA_REGISTRYINDEX);
		if(_L.rawequal(-1, -2)) {
			char* key = &*i;
			_L.pushlightuserdata(key);
			_L.pushnil();
			_L.rawset(LUA_REGISTRYINDEX);
			i = callbacks.erase(i);
		} else
			i++;
		_L.pop(1);
	}
}

function_group::function_group()
{
}

function_group::~function_group()
{
	threads::arlock h(get_lua_lock());
	auto state = fgroup_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->dcallbacks)
		i.second(this);
	fgroup_internal_t::clear(this);
}

void function_group::request_callback(std::function<void(std::string, function*)> cb)
{
	threads::arlock h(get_lua_lock());
	auto state = fgroup_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->functions)
		cb(i.first, i.second);
}

int function_group::add_callback(std::function<void(std::string, function*)> cb,
	std::function<void(function_group*)> dcb)
{
	threads::arlock h(get_lua_lock());
	auto& state = fgroup_internal_t::get(this);
	int handle = state.next_handle++;
	state.callbacks[handle] = cb;
	state.dcallbacks[handle] = dcb;
	for(auto i : state.functions)
		cb(i.first, i.second);
	return handle;
}

void function_group::drop_callback(int handle)
{
	threads::arlock h(get_lua_lock());
	auto state = fgroup_internal_t::get_soft(this);
	if(!state) return;
	state->callbacks.erase(handle);
}

void function_group::do_register(const std::string& name, function& fun)
{
	threads::arlock h(get_lua_lock());
	auto& state = fgroup_internal_t::get(this);
	if(state.functions.count(name)) return;
	state.functions[name] = &fun;
	for(auto i : state.callbacks)
		i.second(name, &fun);
}

void function_group::do_unregister(const std::string& name, function& fun)
{
	threads::arlock h(get_lua_lock());
	auto state = fgroup_internal_t::get_soft(this);
	if(!state || !state->functions.count(name) || state->functions[name] != &fun) return;
	state->functions.erase(name);
	for(auto i : state->callbacks)
		i.second(name, NULL);
}

class_group::class_group()
{
}

class_group::~class_group()
{
	threads::arlock h(get_lua_lock());
	auto state = cgroup_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->dcallbacks)
		i.second(this);
	cgroup_internal_t::clear(this);
}

void class_group::request_callback(std::function<void(std::string, class_base*)> cb)
{
	threads::arlock h(get_lua_lock());
	auto state = cgroup_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->classes)
		cb(i.first, i.second);
}

int class_group::add_callback(std::function<void(std::string, class_base*)> cb,
	std::function<void(class_group*)> dcb)
{
	threads::arlock h(get_lua_lock());
	auto& state = cgroup_internal_t::get(this);
	int handle = state.next_handle++;
	state.callbacks[handle] = cb;
	state.dcallbacks[handle] = dcb;
	for(auto i : state.classes)
		cb(i.first, i.second);
	return handle;
}

void class_group::drop_callback(int handle)
{
	threads::arlock h(get_lua_lock());
	auto state = cgroup_internal_t::get_soft(this);
	if(!state) return;
	state->callbacks.erase(handle);
}

void class_group::do_register(const std::string& name, class_base& fun)
{
	threads::arlock h(get_lua_lock());
	auto& state = cgroup_internal_t::get(this);
	if(state.classes.count(name)) return;
	state.classes[name] = &fun;
	for(auto i : state.callbacks)
		i.second(name, &fun);
}

void class_group::do_unregister(const std::string& name, class_base& fun)
{
	threads::arlock h(get_lua_lock());
	auto state = cgroup_internal_t::get_soft(this);
	if(!state || !state->classes.count(name) || state->classes[name] != &fun) return;
	state->classes.erase(name);
	for(auto i : state->callbacks)
		i.second(name, NULL);
}

std::list<class_ops>& userdata_recogn_fns()
{
	static std::list<class_ops> x;
	return x;
}

std::string try_recognize_userdata(state& state, int index)
{
	for(auto i : userdata_recogn_fns())
		if(i.is(state, index))
			return i.name();
	//Hack: Lua builtin file objects and classobjs.
	if(state.getmetatable(index)) {
		state.pushstring("FILE*");
		state.rawget(LUA_REGISTRYINDEX);
		if(state.rawequal(-1, -2)) {
			state.pop(2);
			return "FILE*";
		}
		state.pop(1);
		state.pushlightuserdata(&classtable_meta_key);
		state.rawget(LUA_REGISTRYINDEX);
		if(state.rawequal(-1, -2)) {
			state.pop(2);
			return "classobj";
		}
		state.pop(1);
	}
	return "unknown";
}

std::string try_print_userdata(state& L, int index)
{
	for(auto i : userdata_recogn_fns())
		if(i.is(L, index))
			return i.print(L, index);
	//Hack: classobjs.
	if(L.getmetatable(index)) {
		L.pushlightuserdata(&classtable_meta_key);
		L.rawget(LUA_REGISTRYINDEX);
		if(L.rawequal(-1, -2)) {
			L.pop(2);
			std::string cname = ((class_info*)L.touserdata(index))->obj->get_name();
			return cname;
		}
		L.pop(1);
	}
	return "no data available";
}

int state::vararg_tag::pushargs(state& L)
{
	int e = 0;
	for(auto i : args) {
		if(i == "")
			L.pushnil();
		else if(i == "true")
			L.pushboolean(true);
		else if(i == "false")
			L.pushboolean(false);
		else if(regex_match("[+-]?(|0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?(0|[1-9][0-9]*))?", i))
			L.pushnumber(strtod(i.c_str(), NULL));
		else if(i[0] == ':')
			L.pushlstring(i.substr(1));
		else
			L.pushlstring(i);
		e++;
	}
	return e;
}

class_base* class_base::lookup(state& L, const std::string& _name)
{
	if(lookup_and_push(L, _name)) {
		class_base* obj = ((class_info*)L.touserdata(-1))->obj;
		L.pop(1);
		return obj;
	}
	return NULL;
}

bool class_base::lookup_and_push(state& L, const std::string& _name)
{
	L.pushlightuserdata(&classtable_key);
	L.rawget(LUA_REGISTRYINDEX);
	if(L.type(-1) == LUA_TNIL) {
		//No classes.
		L.pop(1);
		return false;
	}
	//On top of stack there is class table.
	L.pushlstring(_name);
	L.rawget(-2);
	if(L.type(-1) == LUA_TNIL) {
		//Not found.
		L.pop(2);
		return false;
	}
	L.insert(-2);
	L.pop(1);
	return true;
}

std::set<std::string> class_base::all_classes(state& L)
{
	L.pushlightuserdata(&classtable_key);
	L.rawget(LUA_REGISTRYINDEX);
	if(L.type(-1) == LUA_TNIL) {
		//No classes.
		L.pop(1);
		return std::set<std::string>();
	}
	std::set<std::string> r;
	L.pushnil();
	while(L.next(-2)) {
		L.pop(1);	//Pop value.
		if(L.type(-1) == LUA_TSTRING) r.insert(L.tostring(-1));
	}
	L.pop(1);
	return r;
}

void class_base::register_static(state& L)
{
again:
	L.pushlightuserdata(&classtable_key);
	L.rawget(LUA_REGISTRYINDEX);
	if(L.type(-1) == LUA_TNIL) {
		L.pop(1);
		L.pushlightuserdata(&classtable_key);
		L.newtable();
		L.rawset(LUA_REGISTRYINDEX);
		goto again;
	}
	//On top of stack there is class table.
	L.pushlstring(name);
	L.rawget(-2);
	if(L.type(-1) != LUA_TNIL) {
		//Already registered.
		L.pop(2);
		return;
	}
	L.pop(1);
	L.pushlstring(name);
	//Now construct the object.
	class_info* ci = (class_info*)L.newuserdata(sizeof(class_info));
	ci->obj = this;
again2:
	L.pushlightuserdata(&classtable_meta_key);
	L.rawget(LUA_REGISTRYINDEX);
	if(L.type(-1) == LUA_TNIL) {
		L.pop(1);
		L.pushlightuserdata(&classtable_meta_key);
		L.newtable();
		L.pushstring("__index");
		L.pushlightuserdata(&L);
		L.pushcclosure(class_info::index, 1);
		L.rawset(-3);
		L.pushstring("__newindex");
		L.pushcfunction(class_info::newindex);
		L.rawset(-3);
		L.pushstring("__pairs");
		L.pushlightuserdata(&L);
		L.pushcclosure(class_info::pairs, 1);
		L.rawset(-3);
		L.rawset(LUA_REGISTRYINDEX);
		goto again2;
	}
	L.setmetatable(-2);
	L.rawset(-3);
	L.pop(1);
}

void class_base::delayed_register()
{
	group.do_register(name, *this);
}

functions::functions(function_group& grp, const std::string& basetable, std::initializer_list<entry> fnlist)
{
	std::string base = (basetable == "") ? "" : (basetable + ".");
	for(auto i : fnlist)
		funcs.insert(new fn(grp, base + i.name, i.func));
}

functions::~functions()
{
	for(auto i : funcs)
		delete i;
}

functions::fn::fn(function_group& grp, const std::string& name, std::function<int(state& L, parameters& P)> _func)
	: function(grp, name)
{
	func = _func;
}

functions::fn::~fn() throw()
{
}

int functions::fn::invoke(state& L)
{
	lua::parameters P(L, fname);
	return func(L, P);
}
}
