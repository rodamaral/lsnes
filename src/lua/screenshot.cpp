#include "core/framebuffer.hpp"
#include "lua/internal.hpp"

namespace
{
	function_ptr_luafun lua_gui_screenshot(LS, "gui.screenshot", [](lua_state& L, const std::string& fname) ->
		int {
		std::string fn = L.get_string(1, fname.c_str());
		try {
			take_screenshot(fn);
			return 0;
		} catch(std::exception& e) {
			L.pushstring(e.what());
			L.error();
			return 0;
		}
	});
}
