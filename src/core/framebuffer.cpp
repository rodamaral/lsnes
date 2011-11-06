#include "core/command.hpp"
#include "core/framebuffer.hpp"
#include "core/lua.hpp"
#include "core/misc.hpp"
#include "core/render.hpp"
#include "core/window.hpp"

lcscreen framebuffer;
lcscreen screen_nosignal;
lcscreen screen_corrupt;
extern uint32_t fontdata[];

namespace
{
	struct render_list_entry
	{
		uint32_t codepoint;
		uint32_t x;
		uint32_t y;
		uint32_t scale;
	};

	struct render_list_entry rl_nosignal[] = {
		{'N', 4, 168, 7},
		{'O', 60, 168, 7},
		{'S', 172, 168, 7},
		{'I', 228, 168, 7},
		{'G', 284, 168, 7},
		{'N', 340, 168, 7},
		{'A', 396, 168, 7},
		{'L', 452, 168, 7},
		{0, 0, 0, 0}
	};

	struct render_list_entry rl_corrupt[] = {
		{'S', 88, 56, 7},
		{'Y', 144, 56, 7},
		{'S', 200, 56, 7},
		{'T', 256, 56, 7},
		{'E', 312, 56, 7},
		{'M', 368, 56, 7},
		{'S', 116, 168, 7},
		{'T', 172, 168, 7},
		{'A', 224, 168, 7},
		{'T', 280, 168, 7},
		{'E', 336, 168, 7},
		{'C', 60, 280, 7},
		{'O', 116, 280, 7},
		{'R', 172, 280, 7},
		{'R', 228, 280, 7},
		{'U', 284, 280, 7},
		{'P', 340, 280, 7},
		{'T', 396, 280, 7},
		{0, 0, 0, 0}
	};

	void draw_special_screen(uint32_t* target, struct render_list_entry* rlist)
	{
		while(rlist->scale) {
			int32_t x;
			int32_t y;
			auto g = find_glyph(rlist->codepoint, 0, 0, 0, x, y);
			for(uint32_t j = 0; j < 16; j++) {
				for(uint32_t i = 0; i < 8; i++) {
					uint32_t slice = g.second + j / 4;
					uint32_t bit = 31 - ((j % 4) * 8 + i);
					uint32_t value = (fontdata[slice] >> bit) & 1;
					if(value) {
						uint32_t basex = rlist->x + rlist->scale * i;
						uint32_t basey = rlist->y + rlist->scale * j;
						for(uint32_t j2 = 0; j2 < rlist->scale; j2++)
							for(uint32_t i2 = 0; i2 < rlist->scale; i2++)
								target[(basey + j2) * 512 + (basex + i2)] = 0x7FFFF;
					}
				}
			}
			rlist++;
		}
	}

	void draw_nosignal(uint32_t* target)
	{
		for(unsigned i = 0; i < 512 * 448; i++)
			target[i] = 0x7FC00;
		draw_special_screen(target, rl_nosignal);
	}

	void draw_corrupt(uint32_t* target)
	{
		for(unsigned i = 0; i < 512 * 448; i++)
			target[i] = 0x7FC00;
		draw_special_screen(target, rl_corrupt);
	}

	function_ptr_command<arg_filename> take_screenshot("take-screenshot", "Takes a screenshot",
		"Syntax: take-screenshot <file>\nSaves screenshot to PNG file <file>\n",
		[](arg_filename file) throw(std::bad_alloc, std::runtime_error) {
			framebuffer.save_png(file);
			messages << "Saved PNG screenshot" << std::endl;
		});
}

screen main_screen;


void init_special_screens() throw(std::bad_alloc)
{
	uint32_t buf[512*448];
	draw_nosignal(buf);
	screen_nosignal = lcscreen(buf, 512, 448);
	draw_corrupt(buf);
	screen_corrupt = lcscreen(buf, 512, 448);
}

void redraw_framebuffer()
{
	uint32_t hscl, vscl;
	auto g = get_scale_factors(framebuffer.width, framebuffer.height);
	hscl = g.first;
	vscl = g.second;
	render_queue rq;
	struct lua_render_context lrc;
	lrc.left_gap = 0;
	lrc.right_gap = 0;
	lrc.bottom_gap = 0;
	lrc.top_gap = 0;
	lrc.queue = &rq;
	lrc.width = framebuffer.width * hscl;
	lrc.height = framebuffer.height * vscl;
	lua_callback_do_paint(&lrc);
	main_screen.reallocate(framebuffer.width * hscl + lrc.left_gap + lrc.right_gap, framebuffer.height * vscl +
		lrc.top_gap + lrc.bottom_gap, lrc.left_gap, lrc.top_gap);
	main_screen.copy_from(framebuffer, hscl, vscl);
	//We would want divide by 2, but we'll do it ourselves in order to do mouse.
	window::set_window_compensation(lrc.left_gap, lrc.top_gap, 1, 1);
	rq.run(main_screen);
	window::notify_screen_update();
}

std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height)
{
	uint32_t v = 1;
	uint32_t h = 1;
	if(width < 512)
		h = 2;
	if(height < 400)
		v = 2;
	return std::make_pair(h, v);
}
