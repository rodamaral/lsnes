#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include <stdexcept>

namespace
{
	function_ptr_luafun cbreg(LS, "callback.register", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TFUNCTION)
			throw std::runtime_error("Expected function as 2nd argument to callback.register");
		bool any = false;
		for(auto i : L.get_callbacks()) {
			if(i->get_name() == name) {
				L.pushvalue(2);
				i->_register(L);
				L.pop(1);
				any = true;
			}
		}
		if(!any)
			throw std::runtime_error("Unknown callback type '" + name + "' for callback.register");
		L.pushvalue(2);
		return 1;
	});

	function_ptr_luafun cbunreg(LS, "callback.unregister", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TFUNCTION)
			throw std::runtime_error("Expected function as 2nd argument to callback.unregister");
		bool any = false;
		for(auto i : L.get_callbacks()) {
			if(i->get_name() == name) {
				L.pushvalue(2);
				i->_unregister(L);
				L.pop(1);
				any = true;
			}
		}
		if(!any)
			throw std::runtime_error("Unknown callback type '" + name + "' for callback.unregister");
		L.pushvalue(2);
		return 1;
	});
}
