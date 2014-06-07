#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include <stdexcept>

namespace
{
	class lua_callbacks_list
	{
	public:
		lua_callbacks_list(lua::state& L);
		static size_t overcommit() { return 0; }
		static int create(lua::state& L, lua::parameters& P);
		int index(lua::state& L, lua::parameters& P);
		int newindex(lua::state& L, lua::parameters& P);
	};

	class lua_callback_obj
	{
	public:
		lua_callback_obj(lua::state& L, const std::string& name);
		static size_t overcommit(const std::string& name) { return 0; }
		int _register(lua::state& L, lua::parameters& P);
		int _unregister(lua::state& L, lua::parameters& P);
		int _call(lua::state& L, lua::parameters& P);
		std::string print()
		{
			if(special == 1)
				return "global register";
			else if(special == 2)
				return "global unregister";
			else if(callback)
				return callback->get_name();
			else
				return "(null)";
		}
	private:
		lua::state::callback_list* callback;
		int special;
	};

	lua::_class<lua_callbacks_list> LUA_class_callbacks_list(lua_class_callback, "CALLBACKS_LIST", {
		{"new", lua_callbacks_list::create},
	}, {
		{"__index", &lua_callbacks_list::index},
		{"__newindex", &lua_callbacks_list::newindex},
	});
	lua::_class<lua_callback_obj> LUA_class_callback_obj(lua_class_callback, "CALLBACK_OBJ", {}, {
		{"register", &lua_callback_obj::_register},
		{"unregister", &lua_callback_obj::_unregister},
		{"__call", &lua_callback_obj::_call},
	}, &lua_callback_obj::print);

	lua_callbacks_list::lua_callbacks_list(lua::state& L)
	{
	}

	int lua_callbacks_list::create(lua::state& L, lua::parameters& P)
	{
		lua::_class<lua_callbacks_list>::create(L);
		return 1;
	}

	int lua_callbacks_list::index(lua::state& L, lua::parameters& P)
	{
		std::string name;

		P(P.skipped(), name);

		lua::_class<lua_callback_obj>::create(L, name);
		return 1;
	}

	int lua_callbacks_list::newindex(lua::state& L, lua::parameters& P)
	{
		throw std::runtime_error("Writing is not allowed");
	}

	lua_callback_obj::lua_callback_obj(lua::state& L, const std::string& name)
	{
		callback = NULL;
		special = 0;
		for(auto i : L.get_callbacks())
			if(i->get_name() == name)
				callback = i;
		if(name == "register") {
			special = 1;
			return;
		}
		if(name == "unregister") {
			special = 2;
			return;
		}
		if(!callback && !special)
			throw std::runtime_error("Unknown callback type '" + name + "' for callback.<foo>");
	}

	int lua_callback_obj::_register(lua::state& L, lua::parameters& P)
	{
		int lfn;

		if(!callback) throw std::runtime_error(P.get_fname() + ": not valid");

		P(P.skipped(), P.function(lfn));

		L.pushvalue(lfn);
		callback->_register(L);
		L.pop(1);
		L.pushvalue(lfn);
		return 1;
	}

	int lua_callback_obj::_unregister(lua::state& L, lua::parameters& P)
	{
		int lfn;

		if(!callback) throw std::runtime_error(P.get_fname() + ": not valid");

		P(P.skipped(), P.function(lfn));

		L.pushvalue(lfn);
		callback->_unregister(L);
		L.pop(1);
		L.pushvalue(lfn);
		return 1;
	}

	int lua_callback_obj::_call(lua::state& L, lua::parameters& P)
	{
		std::string name;
		int lfn;

		if(!special) throw std::runtime_error("Need to specify operation to do to callback");

		P(P.skipped(), name, P.function(lfn));

		bool any = false;
		for(auto i : L.get_callbacks()) {
			if(i->get_name() == name) {
				L.pushvalue(lfn);
				if(special == 1)
					i->_register(L);
				else if(special == 2)
					i->_unregister(L);
				L.pop(1);
				any = true;
			}
		}
		if(!any)
			throw std::runtime_error("Unknown callback type '" + name + "' for callback.register");
		L.pushvalue(lfn);
		return 1;
	}
}
