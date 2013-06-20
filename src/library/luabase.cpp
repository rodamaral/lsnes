#include "luabase.hpp"
#include "register-queue.hpp"
#include <iostream>
#include <cassert>

namespace
{
	int lua_trampoline_function(lua_State* L)
	{
		void* ptr = lua_touserdata(L, lua_upvalueindex(1));
		lua_state* state = reinterpret_cast<lua_state*>(lua_touserdata(L, lua_upvalueindex(2)));
		lua_function* f = reinterpret_cast<lua_function*>(ptr);
		lua_state _L(*state, L);
		try {
			return f->invoke(_L);
		} catch(std::exception& e) {
			lua_pushfstring(L, "%s", e.what());
			lua_error(L);
		}
		return 0;
	}

	//Pushes given table to top of stack, creating if needed.
	void recursive_lookup_table(lua_state& L, const std::string& tab)
	{
		if(tab == "") {
#if LUA_VERSION_NUM == 501
			L.pushvalue(LUA_GLOBALSINDEX);
#endif
#if LUA_VERSION_NUM == 502
			L.rawgeti(LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#endif
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

	void register_lua_function(lua_state& L, const std::string& name, lua_function& fun)
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
		void* ptr = reinterpret_cast<void*>(&fun);
		L.pushlightuserdata(ptr);
		L.pushlightuserdata(&L);
		L.pushcclosure(lua_trampoline_function, 2);
		L.setfield(-2, u2.c_str());
		L.pop(1);
	}

	void register_lua_functions(lua_state& L, std::map<std::string, lua_function*>& functions)
	{
		for(auto i : functions)
			register_lua_function(L, i.first, *i.second);
	}
	typedef register_queue<lua_state, lua_function> regqueue_t;
}


lua_state::lua_state() throw(std::bad_alloc)
{
	master = NULL;
	lua_handle = NULL;
	oom_handler = builtin_oom;
	regqueue_t::do_ready(*this, true);
}

lua_state::lua_state(lua_state& _master, lua_State* L)
{
	master = &_master;
	lua_handle = L;
}

lua_state::~lua_state() throw()
{
	if(master)
		return;
	regqueue_t::do_ready(*this, false);
	if(lua_handle)
		lua_close(lua_handle);
}

void lua_state::builtin_oom()
{
	std::cerr << "PANIC: FATAL: Out of memory" << std::endl;
	exit(1);
}

void* lua_state::builtin_alloc(void* user, void* old, size_t olds, size_t news)
{
	if(news) {
		void* m = realloc(old, news);
		if(!m)
			reinterpret_cast<lua_state*>(user)->oom_handler();
		return m;
	} else
		free(old);
	return NULL;
}


lua_function::lua_function(lua_state& _state, const std::string& func) throw(std::bad_alloc)
	: state(_state)
{
	regqueue_t::do_register(state, fname = func, *this);
}

lua_function::~lua_function() throw()
{
	regqueue_t::do_unregister(state, fname);
}

void lua_state::reset() throw(std::bad_alloc, std::runtime_error)
{
	if(master)
		return master->reset();
	if(lua_handle) {
		lua_State* tmp = lua_newstate(lua_state::builtin_alloc, this);
		if(!tmp)
			throw std::runtime_error("Can't re-initialize Lua interpretter");
		lua_close(lua_handle);
		lua_handle = tmp;
	} else {
		//Initialize new.
		lua_handle = lua_newstate(lua_state::builtin_alloc, this);
		if(!lua_handle)
			throw std::runtime_error("Can't initialize Lua interpretter");
	}
	register_lua_functions(*this, functions);
}

void lua_state::deinit() throw()
{
	if(master)
		return master->deinit();
	if(lua_handle)
		lua_close(lua_handle);
	lua_handle = NULL;
}

void lua_state::do_register(const std::string& name, lua_function& fun) throw(std::bad_alloc)
{
	if(master)
		return master->do_register(name, fun);
	functions[name] = &fun;
	if(lua_handle)
		register_lua_function(*this, name, fun);
}

void lua_state::do_unregister(const std::string& name) throw()
{
	if(master)
		return master->do_unregister(name);
	functions.erase(name);
}

bool lua_state::do_once(void* key)
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
