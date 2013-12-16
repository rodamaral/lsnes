#include "core/framebuffer.hpp"
#include "lua/internal.hpp"
#include "lua/bitmap.hpp"

namespace
{
	function_ptr_luafun lua_gui_screenshot(lua_func_misc, "gui.screenshot", [](lua_state& L,
		const std::string& fname) -> int {
		std::string fn = L.get_string(1, fname.c_str());
		take_screenshot(fn);
		return 0;
	});

	function_ptr_luafun lua_gui_screenshot_b(lua_func_misc, "gui.screenshot_bitmap", [](lua_state& L,
		const std::string& fname) -> int {
		framebuffer_raw& _fb = render_get_latest_screen();
		try {
			auto osize = std::make_pair(_fb.get_width(), _fb.get_height());
			std::vector<uint32_t> tmp(_fb.get_width());
			lua_dbitmap* b = lua_class<lua_dbitmap>::create(L, osize.first, osize.second);
			for(size_t y = 0; y < osize.second; y++) {
				_fb.get_format()->decode(&tmp[0], _fb.get_start() + _fb.get_stride() * y,
					_fb.get_width());
				for(size_t x = 0; x < osize.first; x++)
					b->pixels[y * b->width + x] = premultiplied_color(tmp[x]);
			}
		} catch(...) {
			render_get_latest_screen_end();
			throw;
		}
		render_get_latest_screen_end();
		return 1;
	});
}
