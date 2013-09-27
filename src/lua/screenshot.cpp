#include "core/framebuffer.hpp"
#include "lua/internal.hpp"

namespace
{
	function_ptr_luafun lua_gui_screenshot(lua_func_misc, "gui.screenshot", [](lua_state& L,
		const std::string& fname) -> int {
		std::string fn = L.get_string(1, fname.c_str());
		take_screenshot(fn);
		return 0;
	});
}
