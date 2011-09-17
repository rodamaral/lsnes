#include "lua-int.hpp"
#include "command.hpp"
#include "window.hpp"

namespace
{
	function_ptr_luafun lua_print("print", [](lua_State* LS, const std::string& fname) -> int {
		int stacksize = 0;
		while(!lua_isnone(LS, stacksize + 1))
		stacksize++;
		std::string toprint;
		bool first = true;
		for(int i = 0; i < stacksize; i++) {
			size_t len;
			const char* tmp = NULL;
			if(lua_isnil(LS, i + 1)) {
				tmp = "nil";
				len = 3;
			} else if(lua_isboolean(LS, i + 1) && lua_toboolean(LS, i + 1)) {
				tmp = "true";
				len = 4;
			} else if(lua_isboolean(LS, i + 1) && !lua_toboolean(LS, i + 1)) {
				tmp = "false";
				len = 5;
			} else {
				tmp = lua_tolstring(LS, i + 1, &len);
				if(!tmp) {
					tmp = "(unprintable)";
					len = 13;
				}
			}
			std::string localmsg(tmp, tmp + len);
			if(first)
				toprint = localmsg;
			else
				toprint = toprint + "\t" + localmsg;
			first = false;
		}
		window::message(toprint);
		return 0;
	});

	function_ptr_luafun lua_exec("exec", [](lua_State* LS, const std::string& fname) -> int {
		std::string text = get_string_argument(LS, 1, fname.c_str());
		command::invokeC(text);
		return 0;
	});
}
