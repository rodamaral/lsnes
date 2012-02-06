#include "core/framebuffer.hpp"
#include "lua/internal.hpp"

namespace
{
	function_ptr_luafun lua_gui_screenshot("gui.screenshot", [](lua_State* LS, const std::string& fname) -> int {
		std::string fn = get_string_argument(LS, 1, fname.c_str());
		try {
			take_screenshot(fn);
			return 0;
		} catch(std::exception& e) {
			lua_pushstring(LS, e.what());
			lua_error(LS);
			return 0;
		}
	});
}
