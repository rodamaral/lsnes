#include "lua/internal.hpp"
#include "core/keymapper.hpp"
#include "core/command.hpp"
#include <vector>

class lua_inverse_bind
{
public:
	lua_inverse_bind(lua_state& L, const std::string& name, const std::string& cmd);
	std::string print()
	{
		return ikey.getname();
	}
private:
	keyboard::invbind ikey;
};

class lua_command_binding : public command::base
{
public:
	lua_command_binding(lua_state& _L, const std::string& cmd, int idx)
		: command::base(lsnes_cmd, cmd), L(_L)
	{
		L.pushlightuserdata(this);
		L.pushvalue(idx);
		L.rawset(LUA_REGISTRYINDEX);
	}
	void invoke(const std::string& arguments) throw(std::bad_alloc, std::runtime_error)
	{
		L.pushlightuserdata(this);
		L.rawget(LUA_REGISTRYINDEX);
		L.pushstring(arguments.c_str());
		int r = L.pcall(1, 0, 0);
		std::string err;
		if(r == LUA_ERRRUN)
			err = L.get_string(-1, "Lua command callback");
		else if(r == LUA_ERRMEM)
			err = "Out of memory";
		else if(r == LUA_ERRERR)
			err = "Double fault";
		else
			err = "Unknown error";
		if(r) {
			messages << "Error running lua command hook: " << err << std::endl;
		}
	}
private:
	lua_state& L;
};

class lua_command_bind
{
public:
	lua_command_bind(lua_state& L, const std::string& cmd, int idx1, int idx2);
	~lua_command_bind();
	std::string print()
	{
		if(b)
			return a->get_name() + "," + b->get_name();
		else
			return a->get_name();
	}
private:
	lua_command_binding* a;
	lua_command_binding* b;
};

lua_inverse_bind::lua_inverse_bind(lua_state& L, const std::string& name, const std::string& cmd)
	: ikey(lsnes_mapper, cmd, "Lua‣" + name)
{
}

lua_command_bind::lua_command_bind(lua_state& L, const std::string& cmd, int idx1, int idx2)
{
	if(L.type(idx2) == LUA_TFUNCTION) {
		a = new lua_command_binding(L, "+" + cmd, idx1);
		b = new lua_command_binding(L, "-" + cmd, idx2);
	} else {
		a = new lua_command_binding(L, cmd, idx1);
		b = NULL;
	}
}

lua_command_bind::~lua_command_bind()
{
	delete a;
	delete b;
}

namespace
{
	function_ptr_luafun input_bindings(lua_func_misc, "list_bindings", [](lua_state& L, const std::string& fname)
		-> int {
		std::string target;
		if(!L.isnoneornil(1))
			target = L.get_string(1, fname.c_str());
		L.newtable();
		for(auto key : lsnes_mapper.get_bindings()) {
			std::string _key = key;
			std::string cmd = lsnes_mapper.get(key);
			if(target != "" && cmd != target)
				continue;
			L.pushlstring(_key.c_str(), _key.length());
			L.pushlstring(cmd.c_str(), cmd.length());
			L.rawset(-3);
		}
		for(auto key : lsnes_mapper.get_controller_keys()) {
			for(unsigned i = 0;; i++) {
				std::string _key = key->get_string(i);
				if(_key == "")
					break;
				std::string cmd = key->get_command();
				_key = "|/" + _key;
				if(target != "" && cmd != target)
					continue;
				L.pushlstring(_key.c_str(), _key.length());
				L.pushlstring(cmd.c_str(), cmd.length());
				L.rawset(-3);
			}
		}
		return 1;
	});

	function_ptr_luafun get_alias(lua_func_misc, "get_alias", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string a = lsnes_cmd.get_alias_for(name);
		if(a != "")
			L.pushlstring(a.c_str(), a.length());
		else
			L.pushnil();
		return 1;
	});

	function_ptr_luafun set_alias(lua_func_misc, "set_alias", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string value;
		if(L.type(2) != LUA_TNIL)
			value = L.get_string(2, fname.c_str());
		lsnes_cmd.set_alias_for(name, value);
		refresh_alias_binds();
		return 0;
	});

	function_ptr_luafun create_ibind(lua_func_misc, "create_ibind", [](lua_state& L, const std::string& fname)
		-> int {
		std::string name = L.get_string(1, fname.c_str());
		std::string command = L.get_string(2, fname.c_str());
		lua_inverse_bind* b = lua_class<lua_inverse_bind>::create(L, name, command);
		return 1;
	});

	function_ptr_luafun create_cmd(lua_func_misc, "create_command", [](lua_state& L, const std::string& fname)
		-> int {
		if(L.type(2) != LUA_TFUNCTION)
			throw std::runtime_error("Argument 2 of create_command must be function");
		if(L.type(3) != LUA_TFUNCTION && L.type(3) != LUA_TNIL && L.type(3) != LUA_TNONE)
			throw std::runtime_error("Argument 2 of create_command must be function or nil");
		std::string name = L.get_string(1, fname.c_str());
		lua_command_bind* b = lua_class<lua_command_bind>::create(L, name, 2, 3);
		return 1;
	});

}

DECLARE_LUACLASS(lua_inverse_bind, "INVERSEBIND");
DECLARE_LUACLASS(lua_command_bind, "COMMANDBIND");
