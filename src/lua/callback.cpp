#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include <stdexcept>

namespace
{
	class lua_callbacks_list
	{
	public:
		lua_callbacks_list(lua::state& L);
		int index(lua::state& L, const std::string& fname);
		int newindex(lua::state& L, const std::string& fname);
		std::string print()
		{
			return "";
		}
	};

	class lua_callback_obj
	{
	public:
		lua_callback_obj(lua::state& L, const std::string& name);
		int _register(lua::state& L, const std::string& fname);
		int _unregister(lua::state& L, const std::string& fname);
		int _call(lua::state& L, const std::string& fname);
		std::string print()
		{
			if(callback)
				return callback->get_name();
			else
				return "(null)";
		}
	private:
		lua::state::callback_list* callback;
		int special;
	};

	lua::_class<lua_callbacks_list> class_callbacks_list("CALLBACKS_LIST");
	lua::_class<lua_callback_obj> class_callback_obj("CALLBACK_OBJ");

	lua_callbacks_list::lua_callbacks_list(lua::state& L)
	{
		lua::objclass<lua_callbacks_list>().bind_multi(L, {
			{"__index", &lua_callbacks_list::index},
			{"__newindex", &lua_callbacks_list::newindex},
		});
	}

	int lua_callbacks_list::index(lua::state& L, const std::string& fname)
	{
		std::string name = L.get_string(2, fname.c_str());
		lua::_class<lua_callback_obj>::create(L, name);
		return 1;
	}

	int lua_callbacks_list::newindex(lua::state& L, const std::string& fname)
	{
		throw std::runtime_error("Writing is not allowed");
	}

	lua_callback_obj::lua_callback_obj(lua::state& L, const std::string& name)
	{
		lua::objclass<lua_callback_obj>().bind_multi(L, {
			{"register", &lua_callback_obj::_register},
			{"unregister", &lua_callback_obj::_unregister},
			{"__call", &lua_callback_obj::_call},
		});
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

	int lua_callback_obj::_register(lua::state& L, const std::string& fname)
	{
		if(!callback)
			throw std::runtime_error("callback.{,un}register.register not valid");
		if(L.type(2) != LUA_TFUNCTION)
			throw std::runtime_error("Expected function as 2nd argument to callback.<foo>:register");
		L.pushvalue(2);
		callback->_register(L);
		L.pop(1);
		L.pushvalue(2);
		return 1;
	}

	int lua_callback_obj::_unregister(lua::state& L, const std::string& fname)
	{
		if(!callback)
			throw std::runtime_error("callback.{,un}register.unregister not valid");
		if(L.type(2) != LUA_TFUNCTION)
			throw std::runtime_error("Expected function as 2nd argument to callback.<foo>:register");
		L.pushvalue(2);
		callback->_unregister(L);
		L.pop(1);
		L.pushvalue(2);
		return 1;
	}

	int lua_callback_obj::_call(lua::state& L, const std::string& fname)
	{
		if(!special)
			throw std::runtime_error("Need to specify operation to do to callback");
		std::string name = L.get_string(2, "callback.{,un}register");
		if(L.type(3) != LUA_TFUNCTION)
			throw std::runtime_error("Expected function as 2nd argument to callback.{,un}register");
		bool any = false;
		for(auto i : L.get_callbacks()) {
			if(i->get_name() == name) {
				L.pushvalue(3);
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
		L.pushvalue(3);
		return 1;
	}

	lua::fnptr2 callback(lua_func_callback, "callback", [](lua::state& L, lua::parameters& P) -> int {
		lua::_class<lua_callbacks_list>::create(L);
		return 1;
	});
}
