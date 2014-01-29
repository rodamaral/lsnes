#include "lua/internal.hpp"
#include "core/keymapper.hpp"
#include "core/command.hpp"
#include <vector>

class lua_inverse_bind
{
public:
	lua_inverse_bind(lua::state& L, const std::string& name, const std::string& cmd);
	std::string print()
	{
		return ikey.getname();
	}
	static int create(lua::state& L, lua::parameters& P);
private:
	keyboard::invbind ikey;
};

class lua_command_binding : public command::base
{
public:
	lua_command_binding(lua::state& _L, const std::string& cmd, int idx)
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
	lua::state& L;
};

class lua_command_bind
{
public:
	lua_command_bind(lua::state& L, const std::string& cmd, int idx1, int idx2);
	~lua_command_bind();
	std::string print()
	{
		if(b)
			return a->get_name() + "," + b->get_name();
		else
			return a->get_name();
	}
	static int create(lua::state& L, lua::parameters& P);
private:
	lua_command_binding* a;
	lua_command_binding* b;
};

lua_inverse_bind::lua_inverse_bind(lua::state& L, const std::string& name, const std::string& cmd)
	: ikey(lsnes_mapper, cmd, "Lua‣" + name)
{
}

lua_command_bind::lua_command_bind(lua::state& L, const std::string& cmd, int idx1, int idx2)
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
	lua::fnptr2 input_bindings(lua_func_misc, "list_bindings", [](lua::state& L, lua::parameters& P) -> int {
		std::string target;

		P(P.optional(target, ""));

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

	lua::fnptr2 get_alias(lua_func_misc, "get_alias", [](lua::state& L, lua::parameters& P) -> int {
		std::string name;

		P(name);

		std::string a = lsnes_cmd.get_alias_for(name);
		if(a != "")
			L.pushlstring(a);
		else
			L.pushnil();
		return 1;
	});

	lua::fnptr2 set_alias(lua_func_misc, "set_alias", [](lua::state& L, lua::parameters& P) -> int {
		std::string name, value;

		P(name, P.optional(value, ""));

		lsnes_cmd.set_alias_for(name, value);
		refresh_alias_binds();
		return 0;
	});

	lua::_class<lua_inverse_bind> class_inverse_bind(lua_class_bind, "INVERSEBIND", {
		{"new", lua_inverse_bind::create},
	}, {}, &lua_inverse_bind::print);
	lua::_class<lua_command_bind> class_command_bind(lua_class_bind, "COMMANDBIND", {
		{"new", lua_command_bind::create},
	}, {}, &lua_command_bind::print);
}

int lua_inverse_bind::create(lua::state& L, lua::parameters& P)
{
	std::string name, command;

	P(name, command);

	lua_inverse_bind* b = lua::_class<lua_inverse_bind>::create(L, name, command);
	return 1;
}

int lua_command_bind::create(lua::state& L, lua::parameters& P)
{
	std::string name;
	int lfn1 = 0, lfn2 = 0;

	P(name, P.function(lfn1));
	if(P.is_function() || P.is_novalue())
		lfn2 = P.skip();
	else
		P.expected("function or nil");
	lua_command_bind* b = lua::_class<lua_command_bind>::create(L.get_master(), name, lfn1, lfn2);
	return 1;
}
