#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
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

framebuffer::raw screen_corrupt;

namespace
{
	struct render_info
	{
		framebuffer::raw fbuf;
		framebuffer::queue rq;
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

	command::fnptr<command::arg_filename> take_screenshot_cmd(lsnes_cmd, "take-screenshot", "Takes a screenshot",
		"Syntax: take-screenshot <file>\nSaves screenshot to PNG file <file>\n",
		[](command::arg_filename file) throw(std::bad_alloc, std::runtime_error) {
			take_screenshot(file);
			messages << "Saved PNG screenshot" << std::endl;
		});

	settingvar::variable<settingvar::model_int<0, 8191>> dtb(lsnes_vset, "top-border", "UI‣Top padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> dbb(lsnes_vset, "bottom-border",
		"UI‣Bottom padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> dlb(lsnes_vset, "left-border",
		"UI‣Left padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> drb(lsnes_vset, "right-border", "UI‣Right padding",
		0);

	bool last_redraw_no_lua = false;
}

framebuffer::fb<false> main_screen;

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

void redraw_framebuffer(framebuffer::raw& todraw, bool no_lua, bool spontaneous)
{
	uint32_t hscl, vscl;
	auto g = our_rom.rtype->get_scale_factors(todraw.get_width(), todraw.get_height());
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
	lrc.width = todraw.get_width() * hscl;
	lrc.height = todraw.get_height() * vscl;
	if(!no_lua) {
		lua_callback_do_paint(&lrc, spontaneous);
		render_subtitles(lrc);
	}
	ri.fbuf = todraw;
	ri.hscl = hscl;
	ri.vscl = vscl;
	ri.lgap = max(lrc.left_gap, (unsigned)dlb);
	ri.rgap = max(lrc.right_gap, (unsigned)drb);
	ri.tgap = max(lrc.top_gap, (unsigned)dtb);
	ri.bgap = max(lrc.bottom_gap, (unsigned)dbb);
	lsnes_memorywatch.watch(ri.rq);
	buffering.end_write();
	notify_screen_update();
	last_redraw_no_lua = no_lua;
}

void redraw_framebuffer()
{
	render_info& ri = get_read_buffer();
	framebuffer::raw copy = ri.fbuf;
	buffering.end_read();
	//Redraws are never spontaneous
	redraw_framebuffer(copy, last_redraw_no_lua, false);
}


void render_framebuffer()
{
	render_info& ri = get_read_buffer();
	main_screen.reallocate(ri.fbuf.get_width() * ri.hscl + ri.lgap + ri.rgap, ri.fbuf.get_height() * ri.vscl +
		ri.tgap + ri.bgap);
	main_screen.set_origin(ri.lgap, ri.tgap);
	main_screen.copy_from(ri.fbuf, ri.hscl, ri.vscl);
	ri.rq.run(main_screen);
	notify_set_screen(main_screen);
	//We would want divide by 2, but we'll do it ourselves in order to do mouse.
	keyboard::key* mouse_x = lsnes_kbd.try_lookup_key("mouse_x");
	keyboard::key* mouse_y = lsnes_kbd.try_lookup_key("mouse_y");
	keyboard::mouse_calibration xcal;
	keyboard::mouse_calibration ycal;
	xcal.offset = ri.lgap;
	ycal.offset = ri.tgap;
	if(mouse_x && mouse_x->get_type() == keyboard::KBD_KEYTYPE_MOUSE)
		mouse_x->cast_mouse()->set_calibration(xcal);
	if(mouse_y && mouse_y->get_type() == keyboard::KBD_KEYTYPE_MOUSE)
		mouse_y->cast_mouse()->set_calibration(ycal);
	buffering.end_read();
}

std::pair<uint32_t, uint32_t> get_framebuffer_size()
{
	uint32_t v, h;
	render_info& ri = get_read_buffer();
	v = ri.fbuf.get_width();
	h = ri.fbuf.get_height();
	buffering.end_read();
	return std::make_pair(h, v);
}

framebuffer::raw get_framebuffer() throw(std::bad_alloc)
{
	render_info& ri = get_read_buffer();
	framebuffer::raw copy = ri.fbuf;
	buffering.end_read();
	return copy;
}


triplebuffer_logic::triplebuffer_logic() throw(std::bad_alloc)
{
	last_complete_slot = 0;
	read_active = false;
	write_active = false;
	read_active_slot = 0;
	write_active_slot = 0;
}

triplebuffer_logic::~triplebuffer_logic() throw()
{
}

unsigned triplebuffer_logic::start_write() throw()
{
	umutex_class h(mut);
	if(!write_active) {
		//We need to avoid hitting last complete slot or slot that is active for read.
		if(last_complete_slot != 0 && read_active_slot != 0)
			write_active_slot = 0;
		else if(last_complete_slot != 1 && read_active_slot != 1)
			write_active_slot = 1;
		else
			write_active_slot = 2;
	}
	write_active++;
	return write_active_slot;
}

void triplebuffer_logic::end_write() throw()
{
	umutex_class h(mut);
	if(!--write_active)
		last_complete_slot = write_active_slot;
}

unsigned triplebuffer_logic::start_read() throw()
{
	umutex_class h(mut);
	if(!read_active)
		read_active_slot = last_complete_slot;
	read_active++;
	return read_active_slot;
}

void triplebuffer_logic::end_read() throw()
{
	umutex_class h(mut);
	read_active--;
}

void render_kill_request(void* obj)
{
	buffer1.rq.kill_request(obj);
	buffer2.rq.kill_request(obj);
	buffer3.rq.kill_request(obj);
}

framebuffer::raw& render_get_latest_screen()
{
	return get_read_buffer().fbuf;
}

void render_get_latest_screen_end()
{
	buffering.end_read();
}
