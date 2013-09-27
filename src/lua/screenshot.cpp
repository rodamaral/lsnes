#include "core/framebuffer.hpp"
#include "lua/internal.hpp"

namespace
{
	function_ptr_luafun lua_gui_screenshot("gui.screenshot", [](lua_State* LS, const std::string& fname) -> int {
		std::string fn = get_string_argument(LS, 1, fname.c_str());
		take_screenshot(fn);
		return 0;
	});
}
