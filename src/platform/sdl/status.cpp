#include "core/framebuffer.hpp"
#include "platform/sdl/platform.hpp"

void statusarea_model::paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color,
	bool box, uint32_t boxcolor) throw()
{
	if(!w || !h)
		return;
	try {
		//Quickly copy the status area.
		auto& s = platform::get_emustatus();
		std::map<std::string, std::string> newstatus;
		emulator_status::iterator i = s.first();
		while(s.next(i))
			newstatus[i.key] = i.value;
		//Trick to do full redraw.
		if(box) {
			draw_box(surf, x, y, w, h, boxcolor);
			current_contents.clear();
		}
		//Draw it.
		uint32_t lines = h / 16;
		std::map<std::string, std::string>::iterator old_itr = current_contents.begin();
		std::map<std::string, std::string>::iterator new_itr = newstatus.begin();
		for(uint32_t j = 0; j < lines; j++) {
			std::string s;
			if(new_itr == newstatus.end())
				draw_string(surf, "", x, y + 16 * j, w, color);
			else if(old_itr == current_contents.end() || old_itr->first != new_itr->first ||
				old_itr->second != new_itr->second) {
				s = new_itr->first + " " + new_itr->second;
				draw_string(surf, s, x, y + 16 * j, w, color);
			}
			if(old_itr != current_contents.end())
				old_itr++;
			if(new_itr != newstatus.end())
				new_itr++;
		}
		current_contents = newstatus;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

messages_model::messages_model() throw()
{
	first_visible = 0;
	messages_visible = 0;
}

void messages_model::paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color,
	bool box, uint32_t boxcolor) throw()
{
	if(!w || !h)
		return;
	try {
		//Trick to do full redraw.
		if(box) {
			draw_box(surf, x, y, w, h, boxcolor);
			first_visible = 0;
			messages_visible = 0;
		}
		//Quickly copy the messages we need
		std::vector<std::string> msgs;
		uint32_t old_visible_count = messages_visible;
		uint32_t old_painted = messages_visible;
		uint32_t lines = h / 16;
		{
			uint64_t old_first = first_visible;
			mutex::holder h(platform::msgbuf_lock());
			first_visible = platform::msgbuf.get_visible_first();
			msgs.resize(messages_visible = platform::msgbuf.get_visible_count());
			if(old_first == first_visible && messages_visible == old_visible_count)
				return;		//Up to date.
			for(size_t i = 0; i < messages_visible; i++)
				msgs[i] = platform::msgbuf.get_message(first_visible + i);
			//If first changes, we have to repaint all.
			if(old_first != first_visible)
				old_visible_count = 0;
		}
		//Draw messages.
		for(uint32_t i = 0; i < lines; i++) {
			if(i < old_visible_count || (i >= old_painted && i >= messages_visible))
				continue;	//Up to date.
			else if(i < messages_visible) {
				//This is a new message.
				std::ostringstream s;
				s << (first_visible + i) << ": " << msgs[i];
				draw_string(surf, s.str(), x, y + 16 * i, w, color);
			} else {
				//Blank line to paint.
				draw_string(surf, "", x, y + 16 * i, w, color);
			}
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

void paint_modal_dialog(SDL_Surface* surf, const std::string& text, bool confirm, uint32_t color) throw()
{
	try {
		std::string rtext;
		if(confirm)
			rtext = text + "\n\nPress Enter to confirm or Escape to cancel.";
		else
			rtext = text + "\n\nPress Enter or Escape to dismiss.";
		//Find the dimensions of the text.
		uint32_t text_w = 0;
		uint32_t text_h = 0;
		int32_t x = 0;
		int32_t y = 0;
		auto s2 = decode_utf8(rtext);
		for(auto i : s2) {
			auto g = find_glyph(i, x, y, 0, x, y);
			if(x + g.first > text_w)
				text_w = static_cast<uint32_t>(x + g.first);
			if(y + 16 > static_cast<int32_t>(text_h))
				text_h = static_cast<uint32_t>(y + 16);
		}
		uint32_t x1;
		uint32_t x2;
		uint32_t y1;
		uint32_t y2;
		if(text_w + 12 >= static_cast<uint32_t>(surf->w)) {
			x1 = 6;
			x2 = surf->w - 6;
			text_w = x2 - x1;
		} else {
			x1 = (surf->w - text_w) / 2;
			x2 = x1 + text_w;
		}
		if(text_h + 12 >= static_cast<uint32_t>(surf->h)) {
			y1 = 6;
			y2 = surf->h - 6;
			text_h = y2 - y1;
		} else {
			y1 = (surf->h - text_h) / 2;
			y2 = y1 + text_h;
		}
		draw_box(surf, x1, y1, text_w, text_h, color);
		//Draw each line.
		for(uint32_t j = 0; j < text_h / 16; j++) {
			std::string thisline;
			std::string rest;
			size_t split = rtext.find_first_of("\n");
			if(split < rtext.length()) {
				thisline = rtext.substr(0, split);
				rest = rtext.substr(split + 1);
			} else
				thisline = rtext;
			rtext = rest;
			draw_string(surf, thisline, x1, y1 + 16 * j, text_w, color);
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

#define MIN_WIDTH 512
#define DEFAULT_WIDTH 512
#define MIN_HEIGHT 448
#define DEFAULT_HEIGHT 448
#define STATUS_WIDTH 256
#define MESSAGES_LINES 6

screen_layout::screen_layout()
{
	real_width = DEFAULT_WIDTH;
	real_height = DEFAULT_HEIGHT;
	fullscreen_console = false;
	update();
}

void screen_layout::update() throw()
{
	uint32_t w = (real_width < MIN_WIDTH) ? MIN_WIDTH : real_width;
	uint32_t h = (real_height < MIN_HEIGHT) ? MIN_HEIGHT : real_height;
	window_w = w + 22 + STATUS_WIDTH;
	window_h = h + 48 + 16 * MESSAGES_LINES;

	screen_x = 6;
	screen_y = 6;
	status_x = 16 + w;
	status_y = 6;
	messages_x = 6;
	messages_w = window_w - 12;
	commandline_x = 6;
	commandline_y = window_h - 22;
	commandline_w = window_w - 12;

	if(fullscreen_console) {
		screen_w = screen_h = 0;
		status_w = status_h = 0;
		messages_y = 6;
		messages_h = window_h - 38;
	} else {
		screen_w = w;
		screen_h = h;
		status_w = window_w - 22 - w;
		status_h = h;
		messages_y = 16 + h;
		messages_h = window_h - 48 - h;
	}
	mutex::holder hx(platform::msgbuf_lock());
	platform::msgbuf.set_max_window_size(messages_h / 16);
}

void screen_model::set_command_line(commandline_model* c) throw()
{
	cmdline = c;
	repaint_commandline();
}

void screen_model::clear_modal() throw()
{
	modal_active = false;
	modal_confirm = false;
	modal_text = "";
	repaint_full();
}

void screen_model::set_modal(const std::string& text, bool confirm) throw()
{
	try {
		modal_active = true;
		modal_confirm = confirm;
		modal_text = text;
		repaint_modal();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

void screen_model::set_fullscreen_console(bool enable) throw()
{
	layout.fullscreen_console = enable;
	layout.update();
	repaint_full();
}

void screen_model::repaint_commandline() throw()
{
	if(!surf)
		return repaint_full();
	else if(cmdline) {
		SDL_LockSurface(surf);
		cmdline->paint(surf, layout.commandline_x, layout.commandline_y, layout.commandline_w, 0xFFFFFFFFUL);
		SDL_UnlockSurface(surf);
	}
}

void screen_model::repaint_messages() throw()
{
	if(!surf)
		return repaint_full();
	else {
		SDL_LockSurface(surf);
		smessages.paint(surf, layout.messages_x, layout.messages_y, layout.messages_w, layout.messages_h,
			0xFFFFFFFFUL);
		SDL_UnlockSurface(surf);
	}
}

void screen_model::repaint_status() throw()
{
	if(!surf)
		return repaint_full();
	else {
		SDL_LockSurface(surf);
		statusarea.paint(surf, layout.status_x, layout.status_y, layout.status_w, layout.status_h,
			0xFFFFFFFFUL);
		SDL_UnlockSurface(surf);
	}
}

void screen_model::repaint_screen() throw()
{
	if(!surf)
		repaint_full();
	else
		_repaint_screen();
}

void screen_model::flip() throw()
{
	if(!surf)
		repaint_full();
	SDL_Flip(surf);
}

screen_model::screen_model()
{
	surf = NULL;
	cmdline = NULL;
	modal_active = false;
	modal_confirm = false;
	old_screen_h = 0;
	old_screen_w = 0;
}

namespace
{
	void do_set_palette(void* x)
	{
		screen_model* m = reinterpret_cast<screen_model*>(x);
		platform::screen_set_palette(m->pal_r, m->pal_g, m->pal_b);
	}
}

void screen_model::repaint_full() throw()
{
	render_framebuffer();
	uint32_t current_w = main_screen.width;
	uint32_t current_h = main_screen.height;
	if(!surf || old_screen_w != current_w || old_screen_h != current_h) {
		layout.real_width = current_w;
		layout.real_height = current_h;
		layout.update();
		surf = SDL_SetVideoMode(layout.window_w, layout.window_h, 32, SDL_SWSURFACE | SDL_DOUBLEBUF);
		if(!surf) {
			//We are fucked.
			std::cerr << "Can't set video mode: " << SDL_GetError() << std::endl;
			exit(1);
		}
		old_screen_w = current_w;
		old_screen_h = current_h;
		pal_r = surf->format->Rshift;
		pal_g = surf->format->Gshift;
		pal_b = surf->format->Bshift;
		platform::queue(do_set_palette, this, false);
	}
	SDL_LockSurface(surf);
	if(cmdline)
		cmdline->paint(surf, layout.commandline_x, layout.commandline_y, layout.commandline_w, 0xFFFFFFFFUL,
		true, surf->format->Gmask);
	smessages.paint(surf, layout.messages_x, layout.messages_y, layout.messages_w, layout.messages_h,
		0xFFFFFFFFUL, true, surf->format->Gmask);
	statusarea.paint(surf, layout.status_x, layout.status_y, layout.status_w, layout.status_h,
		0xFFFFFFFFUL, true, surf->format->Gmask);
	draw_box(surf, layout.screen_x, layout.screen_y, layout.screen_w, layout.screen_h, surf->format->Gmask);
	if(layout.screen_w && layout.screen_h) {
		for(uint32_t i = 0; i < current_h; i++)
			memcpy(reinterpret_cast<uint8_t*>(surf->pixels) + (i + layout.screen_y) * surf->pitch +
				4 * layout.screen_x, main_screen.rowptr(i), 4 * current_w);
	}
	SDL_UnlockSurface(surf);
	if(modal_active)
		repaint_modal();
}

void screen_model::repaint_modal() throw()
{
	if(!surf)
		return repaint_full();
	else {
		SDL_LockSurface(surf);
		paint_modal_dialog(surf, modal_text, modal_confirm, surf->format->Rmask | (surf->format->Gmask >> 1));
		SDL_UnlockSurface(surf);
	}
}

void screen_model::_repaint_screen() throw()
{
	render_framebuffer();
	uint32_t current_w = main_screen.width;
	uint32_t current_h = main_screen.height;
	//Optimize for case where the screen is not resized.
	{
		SDL_LockSurface(surf);
		if(!surf || old_screen_w != current_w || old_screen_h != current_h)
			goto fail;
		for(uint32_t i = 0; i < current_h; i++)
			memcpy(reinterpret_cast<uint8_t*>(surf->pixels) + (i + layout.screen_y) * surf->pitch +
				4 * layout.screen_x, main_screen.rowptr(i), 4 * current_w);
		SDL_UnlockSurface(surf);
	}
	return;
fail:
	SDL_UnlockSurface(surf);
	repaint_full();
}
