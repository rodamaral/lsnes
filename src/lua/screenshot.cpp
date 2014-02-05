#include "core/framebuffer.hpp"
#include "lua/internal.hpp"
#include "lua/bitmap.hpp"

namespace
{
	int screenshot(lua::state& L, lua::parameters& P)
	{
		std::string filename;

		P(filename);

		take_screenshot(filename);
		return 0;
	}

	int screenshot_bitmap(lua::state& L, lua::parameters& P)
	{
		framebuffer::raw& _fb = render_get_latest_screen();
		try {
			auto osize = std::make_pair(_fb.get_width(), _fb.get_height());
			std::vector<uint32_t> tmp(_fb.get_width());
			lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, osize.first, osize.second);
			for(size_t y = 0; y < osize.second; y++) {
				_fb.get_format()->decode(&tmp[0], _fb.get_start() + _fb.get_stride() * y,
					_fb.get_width());
				for(size_t x = 0; x < osize.first; x++)
					b->pixels[y * b->width + x] = framebuffer::color(tmp[x]);
			}
		} catch(...) {
			render_get_latest_screen_end();
			throw;
		}
		render_get_latest_screen_end();
		return 1;
	}

	lua::functions screenshot_fns(lua_func_misc, "gui", {
		{"screenshot", screenshot},
		{"screenshot_bitmap", screenshot_bitmap},
	});
}
