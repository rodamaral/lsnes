#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/lua.hpp"
#include "core/misc.hpp"
#include "core/render.hpp"
#include "core/triplebuffer.hpp"
#include "core/window.hpp"

lcscreen screen_nosignal;
lcscreen screen_corrupt;

namespace
{
	struct render_info
	{
		lcscreen fbuf;
		render_queue rq;
		uint32_t hscl;
		uint32_t vscl;
		uint32_t lgap;
		uint32_t rgap;
		uint32_t tgap;
		uint32_t bgap;
	};

	triplebuffer_logic buffering;
	render_info buffer1;
	render_info buffer2;
	render_info buffer3;

	render_info& get_write_buffer()
	{
		unsigned i = buffering.start_write();
		switch(i) {
		case 0:
			return buffer1;
		case 1:
			return buffer2;
		case 2:
			return buffer3;
		default:
			return buffer1;
		};
	}

	render_info& get_read_buffer()
	{
		unsigned i = buffering.start_read();
		switch(i) {
		case 0:
			return buffer1;
		case 1:
			return buffer2;
		case 2:
			return buffer3;
		default:
			return buffer1;
		};
	}

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
					uint32_t slice = g.second[j / 4];
					uint32_t bit = 31 - ((j % 4) * 8 + i);
					uint32_t value = (slice >> bit) & 1;
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

	function_ptr_command<arg_filename> take_screenshot_cmd("take-screenshot", "Takes a screenshot",
		"Syntax: take-screenshot <file>\nSaves screenshot to PNG file <file>\n",
		[](arg_filename file) throw(std::bad_alloc, std::runtime_error) {
			take_screenshot(file);
			messages << "Saved PNG screenshot" << std::endl;
		});

	bool last_redraw_no_lua = true;
}

screen main_screen;

void take_screenshot(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	render_info& ri = get_read_buffer();
	ri.fbuf.save_png(file);
	buffering.end_read();
}


void init_special_screens() throw(std::bad_alloc)
{
	std::vector<uint32_t> buf;
	buf.resize(512*448);
	draw_nosignal(&buf[0]);
	screen_nosignal = lcscreen(&buf[0], 512, 448);
	draw_corrupt(&buf[0]);
	screen_corrupt = lcscreen(&buf[0], 512, 448);
}

void redraw_framebuffer(lcscreen& todraw, bool no_lua)
{
	uint32_t hscl, vscl;
	auto g = get_scale_factors(todraw.width, todraw.height);
	hscl = g.first;
	vscl = g.second;
	render_info& ri = get_write_buffer();
	ri.rq.clear();
	struct lua_render_context lrc;
	lrc.left_gap = 0;
	lrc.right_gap = 0;
	lrc.bottom_gap = 0;
	lrc.top_gap = 0;
	lrc.queue = &ri.rq;
	lrc.width = todraw.width * hscl;
	lrc.height = todraw.height * vscl;
	if(!no_lua)
		lua_callback_do_paint(&lrc);
	ri.fbuf = todraw;
	ri.hscl = hscl;
	ri.vscl = vscl;
	ri.lgap = lrc.left_gap;
	ri.rgap = lrc.right_gap;
	ri.tgap = lrc.top_gap;
	ri.bgap = lrc.bottom_gap;
	buffering.end_write();
	information_dispatch::do_screen_update();
	last_redraw_no_lua = no_lua;
}

void redraw_framebuffer()
{
	render_info& ri = get_read_buffer();
	lcscreen copy = ri.fbuf;
	buffering.end_read();
	redraw_framebuffer(copy, last_redraw_no_lua);
}


void render_framebuffer()
{
	static uint32_t val1, val2, val3, val4;
	uint32_t nval1, nval2, nval3, nval4;
	render_info& ri = get_read_buffer();
	main_screen.reallocate(ri.fbuf.width * ri.hscl + ri.lgap + ri.rgap, ri.fbuf.height * ri.vscl + ri.tgap +
		ri.bgap);
	main_screen.set_origin(ri.lgap, ri.tgap);
	main_screen.copy_from(ri.fbuf, ri.hscl, ri.vscl);
	ri.rq.run(main_screen);
	information_dispatch::do_set_screen(main_screen);
	//We would want divide by 2, but we'll do it ourselves in order to do mouse.
	keygroup* mouse_x = keygroup::lookup_by_name("mouse_x");
	keygroup* mouse_y = keygroup::lookup_by_name("mouse_y");
	nval1 = ri.lgap;
	nval2 = ri.tgap;
	nval3 = ri.fbuf.width * ri.hscl + ri.rgap;
	nval4 = ri.fbuf.height * ri.vscl + ri.bgap;
	if(mouse_x && (nval1 != val1 || nval3 != val3))
		mouse_x->change_calibration(-static_cast<short>(ri.lgap), ri.lgap, ri.fbuf.width * ri.hscl + ri.rgap,
			0.5);
	if(mouse_y && (nval2 != val2 || nval4 != val4))
		mouse_y->change_calibration(-static_cast<short>(ri.tgap), ri.tgap, ri.fbuf.height * ri.vscl + ri.bgap,
			0.5);
	val1 = nval1;
	val2 = nval2;
	val3 = nval3;
	val4 = nval4;
	buffering.end_read();
}

std::pair<uint32_t, uint32_t> get_framebuffer_size()
{
	uint32_t v, h;
	render_info& ri = get_read_buffer();
	v = ri.fbuf.width;
	h = ri.fbuf.height;
	buffering.end_read();
	return std::make_pair(h, v);
}

lcscreen get_framebuffer() throw(std::bad_alloc)
{
	render_info& ri = get_read_buffer();
	lcscreen copy = ri.fbuf;
	buffering.end_read();
	return copy;
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
