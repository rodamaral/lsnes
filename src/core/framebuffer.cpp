#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "core/memorywatch.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/subtitles.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "lua/lua.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/framebuffer-pixfmt-lrgb.hpp"
#include "library/minmax.hpp"
#include "library/triplebuffer.hpp"

framebuffer::raw screen_corrupt;
void update_movie_state();

namespace
{
	struct render_list_entry
	{
		uint32_t codepoint;
		uint32_t x;
		uint32_t y;
		uint32_t scale;
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
			auto g = main_font.get_glyph(rlist->codepoint);
			for(uint32_t j = 0; j < 16; j++) {
				for(uint32_t i = 0; i < (g.wide ? 16 : 8); i++) {
					uint32_t slice = g.data[j / (g.wide ? 2 : 4)];
					uint32_t bit = 31 - ((j % (g.wide ? 2 : 4)) * (g.wide ? 16 : 8) + i);
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

	void draw_corrupt(uint32_t* target)
	{
		for(unsigned i = 0; i < 512 * 448; i++)
			target[i] = 0x7FC00;
		draw_special_screen(target, rl_corrupt);
	}

	command::fnptr<command::arg_filename> take_screenshot_cmd(lsnes_cmds, "take-screenshot", "Takes a screenshot",
		"Syntax: take-screenshot <file>\nSaves screenshot to PNG file <file>\n",
		[](command::arg_filename file) throw(std::bad_alloc, std::runtime_error) {
			CORE().fbuf.take_screenshot(file);
			messages << "Saved PNG screenshot" << std::endl;
		});

	settingvar::supervariable<settingvar::model_int<0, 8191>> dtb(lsnes_setgrp, "top-border",
		"UI‣Top padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> dbb(lsnes_setgrp, "bottom-border",
		"UI‣Bottom padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> dlb(lsnes_setgrp, "left-border",
		"UI‣Left padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> drb(lsnes_setgrp, "right-border",
		"UI‣Right padding", 0);
}

framebuffer::raw emu_framebuffer::screen_corrupt;

emu_framebuffer::emu_framebuffer()
	: buffering(buffer1, buffer2, buffer3)
{
	last_redraw_no_lua = false;
}

void emu_framebuffer::take_screenshot(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	render_info& ri = buffering.get_read();
	ri.fbuf.save_png(file);
	buffering.put_read();
}


void emu_framebuffer::init_special_screens() throw(std::bad_alloc)
{
	std::vector<uint32_t> buf;
	buf.resize(512*448);

	framebuffer::info inf;
	inf.type = &framebuffer::pixfmt_lrgb;
	inf.mem = reinterpret_cast<char*>(&buf[0]);
	inf.physwidth = 512;
	inf.physheight = 448;
	inf.physstride = 2048;
	inf.width = 512;
	inf.height = 448;
	inf.stride = 2048;
	inf.offset_x = 0;
	inf.offset_y = 0;

	draw_corrupt(&buf[0]);
	screen_corrupt = framebuffer::raw(inf);
}

void emu_framebuffer::redraw_framebuffer(framebuffer::raw& todraw, bool no_lua, bool spontaneous)
{
	uint32_t hscl, vscl;
	auto g = our_rom.rtype->get_scale_factors(todraw.get_width(), todraw.get_height());
	hscl = g.first;
	vscl = g.second;
	render_info& ri = buffering.get_write();
	ri.rq.clear();
	struct lua::render_context lrc;
	lrc.left_gap = 0;
	lrc.right_gap = 0;
	lrc.bottom_gap = 0;
	lrc.top_gap = 0;
	lrc.queue = &ri.rq;
	lrc.width = todraw.get_width() * hscl;
	lrc.height = todraw.get_height() * vscl;
	if(!no_lua) {
		lua_callback_do_paint(&lrc, spontaneous);
		CORE().subtitles.render(lrc);
	}
	ri.fbuf = todraw;
	ri.hscl = hscl;
	ri.vscl = vscl;
	ri.lgap = max(lrc.left_gap, (unsigned)dlb(CORE().settings));
	ri.rgap = max(lrc.right_gap, (unsigned)drb(CORE().settings));
	ri.tgap = max(lrc.top_gap, (unsigned)dtb(CORE().settings));
	ri.bgap = max(lrc.bottom_gap, (unsigned)dbb(CORE().settings));
	CORE().mwatch.watch(ri.rq);
	buffering.put_write();
	notify_screen_update();
	last_redraw_no_lua = no_lua;
	update_movie_state();
}

void emu_framebuffer::redraw_framebuffer()
{
	render_info& ri = buffering.get_read();
	framebuffer::raw copy = ri.fbuf;
	buffering.put_read();
	//Redraws are never spontaneous
	redraw_framebuffer(copy, last_redraw_no_lua, false);
}

void emu_framebuffer::render_framebuffer()
{
	render_info& ri = buffering.get_read();
	main_screen.reallocate(ri.fbuf.get_width() * ri.hscl + ri.lgap + ri.rgap, ri.fbuf.get_height() * ri.vscl +
		ri.tgap + ri.bgap);
	main_screen.set_origin(ri.lgap, ri.tgap);
	main_screen.copy_from(ri.fbuf, ri.hscl, ri.vscl);
	ri.rq.run(main_screen);
	notify_set_screen(main_screen);
	//We would want divide by 2, but we'll do it ourselves in order to do mouse.
	keyboard::key* mouse_x = CORE().keyboard.try_lookup_key("mouse_x");
	keyboard::key* mouse_y = CORE().keyboard.try_lookup_key("mouse_y");
	keyboard::mouse_calibration xcal;
	keyboard::mouse_calibration ycal;
	xcal.offset = ri.lgap;
	ycal.offset = ri.tgap;
	if(mouse_x && mouse_x->get_type() == keyboard::KBD_KEYTYPE_MOUSE)
		mouse_x->cast_mouse()->set_calibration(xcal);
	if(mouse_y && mouse_y->get_type() == keyboard::KBD_KEYTYPE_MOUSE)
		mouse_y->cast_mouse()->set_calibration(ycal);
	buffering.put_read();
}

std::pair<uint32_t, uint32_t> emu_framebuffer::get_framebuffer_size()
{
	uint32_t v, h;
	render_info& ri = buffering.get_read();
	v = ri.fbuf.get_width();
	h = ri.fbuf.get_height();
	buffering.put_read();
	return std::make_pair(h, v);
}

framebuffer::raw emu_framebuffer::get_framebuffer() throw(std::bad_alloc)
{
	render_info& ri = buffering.get_read();
	framebuffer::raw copy = ri.fbuf;
	buffering.put_read();
	return copy;
}

void emu_framebuffer::render_kill_request(void* obj)
{
	buffer1.rq.kill_request(obj);
	buffer2.rq.kill_request(obj);
	buffer3.rq.kill_request(obj);
}

framebuffer::raw& emu_framebuffer::render_get_latest_screen()
{
	return buffering.get_read().fbuf;
}

void emu_framebuffer::render_get_latest_screen_end()
{
	buffering.put_read();
}
