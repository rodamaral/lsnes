#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/window.hpp"

#include "plat-sdl/paint.hpp"

#define MAXMESSAGES 6

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define WRITE3(ptr, idx, c) do {\
	(ptr)[(idx) + 0] = (c) >> 16; \
	(ptr)[(idx) + 1] = (c) >> 8; \
	(ptr)[(idx) + 2] = (c); \
	} while(0)
#else
#define WRITE3(ptr, idx, c) do {\
	(ptr)[(idx) + 0] = (c); \
	(ptr)[(idx) + 1] = (c) >> 8; \
	(ptr)[(idx) + 2] = (c) >> 16; \
	} while(0)
#endif

namespace
{
	SDL_Surface* hwsurf;
	screen* our_screen;
	sdlw_display_parameters dp;
	bool fullscreen_console_active;
	bool video_locked;
	bool screen_paintable;
	bool screen_dirty;
	bool modal_dialog_shown;
	std::string modal_dialog_text;
	bool modal_dialog_confirm;
	bool have_received_frames;

	void paint_line(uint8_t* ptr, uint32_t length, uint32_t step, uint32_t pbytes, uint32_t color)
	{
		switch(pbytes) {
		case 1:
			for(uint32_t i = 0; i < length; i++) {
				*ptr = color;
				ptr += step;
			}
			break;
		case 2:
			for(uint32_t i = 0; i < length; i++) {
				*reinterpret_cast<uint16_t*>(ptr) = color;
				ptr += step;
			}
			break;
		case 3:
			for(uint32_t i = 0; i < length; i++) {
				WRITE3(ptr, 0, color);
				ptr += step;
			}
			break;
		case 4:
			for(uint32_t i = 0; i < length; i++) {
				*reinterpret_cast<uint32_t*>(ptr) = color;
				ptr += step;
			}
			break;
		}
	}

	std::vector<uint32_t> decode_utf8(std::string s)
	{
		std::vector<uint32_t> ret;
		for(auto i = s.begin(); i != s.end(); i++) {
			uint32_t j = static_cast<uint8_t>(*i);
			if(j < 128)
				ret.push_back(j);
			else if(j < 192)
				continue;
			else if(j < 224) {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 192) * 64 + (j2 - 128));
			} else if(j < 240) {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				uint32_t j3 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 224) * 4096 + (j2 - 128) * 64 + (j3 - 128));
			} else {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				uint32_t j3 = static_cast<uint8_t>(*(++i));
				uint32_t j4 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 240) * 262144 + (j2 - 128) * 4096 + (j3 - 128) * 64 + (j4 - 128));
			}
		}
		return ret;
	}

	inline void draw_blank_glyph_3(uint8_t* base, uint32_t pitch, uint32_t w, uint32_t color, uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			uint8_t* ptr = base + j * pitch;
			uint32_t c = (j >= curstart) ? color : 0;
			for(uint32_t i = 0; i < w; i++)
				WRITE3(ptr, 3 * i, c);
		}
	}

	template<typename T>
	inline void draw_blank_glyph_T(uint8_t* base, uint32_t pitch, uint32_t w, uint32_t color, uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			T* ptr = reinterpret_cast<T*>(base + j * pitch);
			T c = (j >= curstart) ? color : 0;
			for(uint32_t i = 0; i < w; i++)
				ptr[i] = c;
		}
	}

	template<bool wide>
	inline void draw_glyph_3(uint8_t* base, uint32_t pitch, uint32_t w, const uint32_t* gdata, uint32_t color,
		uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			uint8_t* ptr = base + j * pitch;
			uint32_t bgc = (j >= curstart) ? color : 0;
			uint32_t fgc = (j >= curstart) ? 0 : color;
			uint32_t dataword = gdata[j >> (wide ? 1 : 2)];
			unsigned rbit = (~(j << (wide ? 4 : 3))) & 0x1F;
			for(uint32_t i = 0; i < w; i++) {
				bool b = (((dataword >> (rbit - i)) & 1));
				WRITE3(ptr, 3 * i, b ? fgc : bgc);
			}
		}
	}

	template<typename T, bool wide>
	inline void draw_glyph_T(uint8_t* base, uint32_t pitch, uint32_t w, const uint32_t* gdata, uint32_t color,
		uint32_t curstart)
	{
		for(uint32_t j = 0; j < 16; j++) {
			T* ptr = reinterpret_cast<T*>(base + j * pitch);
			T bgc = (j >= curstart) ? color : 0;
			T fgc = (j >= curstart) ? 0 : color;
			uint32_t dataword = gdata[j >> (wide ? 1 : 2)];
			unsigned rbit = (~(j << (wide ? 4 : 3))) & 0x1F;
			for(uint32_t i = 0; i < w; i++) {
				bool b = (((dataword >> (rbit - i)) & 1));
				ptr[i] = b ? fgc : bgc;
			}
		}
	}


	void draw_blank_glyph(uint8_t* base, uint32_t pitch, uint32_t pbytes, uint32_t w, uint32_t wleft,
		uint32_t color, uint32_t hilite_mode)
	{
		if(w > wleft)
			w = wleft;
		if(!w)
			return;
		uint32_t curstart = 16;
		if(hilite_mode == 1)
			curstart = 14;
		if(hilite_mode == 2)
			curstart = 0;
		switch(pbytes) {
		case 1:
			draw_blank_glyph_T<uint8_t>(base, pitch, w, color, curstart);
			break;
		case 2:
			draw_blank_glyph_T<uint16_t>(base, pitch, w, color, curstart);
			break;
		case 3:
			draw_blank_glyph_3(base, pitch, w, color, curstart);
			break;
		case 4:
			draw_blank_glyph_T<uint32_t>(base, pitch, w, color, curstart);
			break;
		}
	}

	void draw_glyph(uint8_t* base, uint32_t pitch, uint32_t pbytes, const uint32_t* gdata, uint32_t w,
		uint32_t wleft, uint32_t color, uint32_t hilite_mode)
	{
		bool wide = (w > 8);
		if(w > wleft)
			w = wleft;
		if(!w)
			return;
		uint32_t curstart = 16;
		if(hilite_mode == 1)
			curstart = 14;
		if(hilite_mode == 2)
			curstart = 0;
		switch(pbytes) {
		case 1:
			if(wide)
				draw_glyph_T<uint8_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint8_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		case 2:
			if(wide)
				draw_glyph_T<uint16_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint16_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		case 3:
			if(wide)
				draw_glyph_3<true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_3<false>(base, pitch, w, gdata, color, curstart);
			break;
		case 4:
			if(wide)
				draw_glyph_T<uint32_t, true>(base, pitch, w, gdata, color, curstart);
			else
				draw_glyph_T<uint32_t, false>(base, pitch, w, gdata, color, curstart);
			break;
		}
	}

	void draw_string(uint8_t* base, uint32_t pitch, uint32_t pbytes, std::vector<uint32_t> s, uint32_t x,
		uint32_t y, uint32_t maxwidth, uint32_t color, uint32_t hilite_mode = 0, uint32_t hilite_pos = 0)
	{
		int32_t pos_x = 0;
		int32_t pos_y = 0;
		size_t xo = pbytes;
		size_t yo = pitch;
		unsigned c = 0;
		for(auto si : s) {
			uint32_t old_x = pos_x;
			uint32_t old_y = pos_y;
			auto g = find_glyph(si, pos_x, pos_y, 0, pos_x, pos_y);
			uint32_t mw = maxwidth - old_x;
			if(maxwidth < old_x)
				mw = 0;
			if(mw > g.first)
				mw = g.first;
			uint8_t* cbase = base + (y + old_y) * yo + (x + old_x) * xo;
			if(g.second == NULL)
				draw_blank_glyph(cbase, pitch, pbytes, g.first, mw, color,
					(c == hilite_pos) ? hilite_mode : 0);
			else
				draw_glyph(cbase, pitch, pbytes, g.second, g.first, mw, color,
					(c == hilite_pos) ? hilite_mode : 0);
			c++;
		}
		if(c == hilite_pos) {
			uint32_t old_x = pos_x;
			uint32_t mw = maxwidth - old_x;
			if(maxwidth < old_x)
				mw = 0;
			draw_blank_glyph(base + y * yo + (x + old_x) * xo, pitch, pbytes, 8, mw, 0xFFFFFFFFU,
				hilite_mode);
			pos_x += 8;
		}
		if(pos_x < maxwidth)
			draw_blank_glyph(base + y * yo + (x + pos_x) * xo, pitch, pbytes, maxwidth - pos_x,
				maxwidth - pos_x, 0, 0);
	}

	void draw_string(uint8_t* base, uint32_t pitch, uint32_t pbytes, std::string s, uint32_t x, uint32_t y,
		uint32_t maxwidth, uint32_t color, uint32_t hilite_mode = 0, uint32_t hilite_pos = 0)
	{
		draw_string(base, pitch, pbytes, decode_utf8(s), x, y, maxwidth, color, hilite_mode, hilite_pos);
	}
}

void draw_box(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
	if(!w || !h)
		return;
	uint32_t sx, sy, ex, ey;
	uint32_t bsx, bsy, bex, bey;
	uint32_t pbytes = surf->format->BytesPerPixel;
	uint32_t lstride = surf->pitch;
	uint8_t* p = reinterpret_cast<uint8_t*>(surf->pixels);
	size_t xo = surf->format->BytesPerPixel;
	size_t yo = surf->pitch;
	sx = (x < 6) ? 0 : (x - 6);
	sy = (y < 6) ? 0 : (y - 6);
	ex = (x + w + 6 > surf->w) ? surf->w : (x + w + 6);
	ey = (y + h + 6 > surf->h) ? surf->h : (y + h + 6);
	bsx = (x < 4) ? 0 : (x - 4);
	bsy = (y < 4) ? 0 : (y - 4);
	bex = (x + w + 4 > surf->w) ? surf->w : (x + w + 4);
	bey = (y + h + 4 > surf->h) ? surf->h : (y + h + 4);
	//First, blank the area.
	for(uint32_t j = sy; j < ey; j++)
		memset(p + j * yo + sx * xo, 0, (ex - sx) * xo);
	//Paint the borders.
	if(x >= 4)
		paint_line(p + bsy * yo + (x - 4) * xo, bey - bsy, yo, xo, color);
	if(x >= 3)
		paint_line(p + bsy * yo + (x - 3) * xo, bey - bsy, yo, xo, color);
	if(y >= 4)
		paint_line(p + (y - 4) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(y >= 3)
		paint_line(p + (y - 3) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(x + w + 3 < surf->w)
		paint_line(p + bsy * yo + (x + w + 2) * xo, bey - bsy, yo, xo, color);
	if(x + w + 4 < surf->w)
		paint_line(p + bsy * yo + (x + w + 3) * xo, bey - bsy, yo, xo, color);
	if(y + h + 3 < surf->h)
		paint_line(p + (y + h + 2) * yo + bsx * xo, bex - bsx, xo, xo, color);
	if(y + h + 4 < surf->h)
		paint_line(p + (y + h + 3) * yo + bsx * xo, bex - bsx, xo, xo, color);
}

sdlw_display_parameters::sdlw_display_parameters()
{
	real_screen_w = 0;
	real_screen_h = 0;
	fullscreen_console = false;
	virtual_screen_w = 512;
	virtual_screen_h = 448;
	display_w = virtual_screen_w + 278;
	display_h = virtual_screen_h + 16 * MAXMESSAGES + 48;
	screenarea_x = 6;
	screenarea_y = 6;
	statusarea_x = virtual_screen_w + 16;
	statusarea_y = 6;
	messagearea_x = 6;
	cmdline_x = 6;
	cmdline_y = display_h - 22;
	cmdline_w = display_w - 12;
	messagearea_w = display_w - 12;
	screenarea_w = virtual_screen_w;
	screenarea_h = virtual_screen_h;
	statusarea_w = 256;
	statusarea_lines = virtual_screen_h / 16;
	messagearea_y = virtual_screen_h + 16;
	messagearea_lines = MAXMESSAGES;
	messagearea_trailing_y = messagearea_y + MAXMESSAGES;
	messagearea_trailing_h = 0;
	statusarea_h = statusarea_lines * 16;
	messagearea_h = messagearea_lines * 16 + messagearea_trailing_h;
}

sdlw_display_parameters::sdlw_display_parameters(uint32_t rscrw, uint32_t rscrh, bool cactive)
{
	real_screen_w = rscrw;
	real_screen_h = rscrh;
	fullscreen_console = cactive;
	virtual_screen_w = (((real_screen_w < 512) ? 512 : real_screen_w) + 15) >> 4 << 4;
	virtual_screen_h = (((real_screen_w < 448) ? 448 : real_screen_h) + 15) >> 4 << 4;
	display_w = virtual_screen_w + 278;
	display_h = virtual_screen_h + 16 * MAXMESSAGES + 48;
	screenarea_x = 6;
	screenarea_y = 6;
	statusarea_x = virtual_screen_w + 16;
	statusarea_y = 6;
	messagearea_x = 6;
	cmdline_x = 6;
	cmdline_y = display_h - 22;
	cmdline_w = display_w - 12;
	messagearea_w = display_w - 12;
	if(fullscreen_console) {
		screenarea_w = 0;
		screenarea_h = 0;
		statusarea_w = 0;
		statusarea_lines = 0;
		messagearea_y = 6;
		messagearea_lines = (display_h - 38) / 16;
		messagearea_trailing_y = messagearea_y + 16 * messagearea_lines;
		messagearea_trailing_h = (display_h - 38) % 16;
	} else {
		screenarea_w = virtual_screen_w;
		screenarea_h = virtual_screen_h;
		statusarea_w = 256;
		statusarea_lines = virtual_screen_h / 16;
		messagearea_y = virtual_screen_h + 16;
		messagearea_lines = MAXMESSAGES;
		messagearea_trailing_y = messagearea_y + MAXMESSAGES;
		messagearea_trailing_h = 0;
	}
	statusarea_h = statusarea_lines * 16;
	messagearea_h = messagearea_lines * 16 + messagearea_trailing_h;
}

bool paint_command(SDL_Surface* surf, const sdlw_display_parameters& p, bool full)
{
	static bool cached_command_active = false;
	static bool cached_overwrite = false;
	static uint32_t cached_rawcursor = 0;
	static std::string cached_command;

	//Query status of command line.
	auto cmd = get_current_command();

	if(full) {
		draw_box(surf, p.cmdline_x, p.cmdline_y, p.cmdline_w, 16, surf->format->Gmask);
		cached_command_active = false;
		cached_overwrite = false;
		cached_rawcursor = 0;
		cached_command = "";
	}
	if(cached_command_active == cmd.active && cached_overwrite == cmd.overwrite &&
		cached_rawcursor == cmd.rawpos && cached_command == cmd.encoded)
		return full;
	try {
		if(cmd.active) {
			//FIXME, scroll text if too long.
			uint32_t hilite_mode = cmd.overwrite ? 2 : 1;
			auto s2 = decode_utf8(sdlw_decode_string(cmd.encoded));
			draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, surf->format->BytesPerPixel,
				s2, p.cmdline_x, p.cmdline_y, p.cmdline_w, 0xFFFFFFFFU, hilite_mode, cmd.rawpos / 4);
		} else
			draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, surf->format->BytesPerPixel,
				"", p.cmdline_x, p.cmdline_y, p.cmdline_w, 0xFFFFFFFFU);
		cached_command_active = cmd.active;
		cached_overwrite = cmd.overwrite;
		cached_rawcursor = cmd.rawpos;
		cached_command = cmd.encoded;
	} catch(...) {
	}
	return true;
}

bool paint_messages(SDL_Surface* surf, const sdlw_display_parameters& p, bool full)
{
	static uint64_t cached_first = 0;
	static uint64_t cached_messages = 0;
	if(full) {
		draw_box(surf, p.messagearea_x, p.messagearea_y, p.messagearea_w, p.messagearea_h,
			surf->format->Gmask);
		cached_first = 0;
		cached_messages = 0;
	}
	uint32_t message_y = p.messagearea_y;
	size_t maxmessages = window::msgbuf.get_max_window_size();
	size_t msgnum = window::msgbuf.get_visible_first();
	size_t visible = window::msgbuf.get_visible_count();
	if((!visible && cached_messages == 0) || (msgnum == cached_first && visible == cached_messages))
		return full;
	if(cached_messages == 0)
		cached_first = msgnum;		//Little trick.
	for(size_t j = 0; j < maxmessages; j++)
		try {
			if(msgnum == cached_first && j < cached_messages)
				continue;	//Don't draw lines that stayed the same.
			if(j >= visible && j >= cached_messages)
				continue;	//Don't draw blank lines.
			std::ostringstream o;
			if(j < visible)
				o << (msgnum + j + 1) << ": " << window::msgbuf.get_message(msgnum + j);
			draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch,
				surf->format->BytesPerPixel, o.str(), p.messagearea_x,
				message_y + 16 * j, p.messagearea_w, 0xFFFFFFFFU);
		} catch(...) {
		}
	if(window::msgbuf.is_more_messages())
		try {
			draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch,
				surf->format->BytesPerPixel, "--More--",
				p.messagearea_x + p.messagearea_w - 64, message_y + 16 * maxmessages - 16, 64,
				0xFFFFFFFFU);
		} catch(...) {
		}
	cached_first = msgnum;
	cached_messages = visible;
	return true;
}

bool paint_status(SDL_Surface* surf, const sdlw_display_parameters& p, bool full)
{
	auto& status = window::get_emustatus();
	try {
		std::ostringstream y;
		y << get_framerate();
		status["FPS"] = y.str();
	} catch(...) {
	}
	if(!p.statusarea_w || !p.statusarea_h)
		return full;
	uint32_t y = p.statusarea_y;
	uint32_t lines = 0;
	static std::map<std::string, std::string> old_status;
	size_t old_lines = old_status.size();
	if(full) {
		old_status.clear();
		draw_box(surf, p.statusarea_x, p.statusarea_y, p.statusarea_w, p.statusarea_h,
			surf->format->Gmask);
	}
	bool paint_any = false;
	bool desynced = false;
	for(auto i : status) {
		if(!old_status.count(i.first))
			desynced = true;
		if(!desynced && old_status[i.first] == i.second) {
			y += 16;
			lines++;
			continue;
		}
		//Paint in full.
		paint_any = true;
		try {
			std::string str = i.first + " " + i.second;
			draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch,
				surf->format->BytesPerPixel, str, p.statusarea_x, y, p.statusarea_w,
				0xFFFFFFFFU);
			old_status[i.first] = i.second;
		} catch(...) {
		}
		y += 16;
		lines++;
	}
	for(size_t i = lines; i < old_lines; i++) {
		draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch,
			surf->format->BytesPerPixel, "", p.statusarea_x, y, p.statusarea_w, 0);
		y += 16;
	}
	return paint_any || full;
}

bool paint_screen(SDL_Surface* surf, const sdlw_display_parameters& p, bool full)
{
	if(full) {
		draw_box(surf, p.screenarea_x, p.screenarea_y, p.screenarea_w, p.screenarea_h,
			surf->format->Gmask);
	}
}

void paint_modal_dialog(SDL_Surface* surf, const std::string& text, bool confirm)
{
	std::string realtext;
	if(confirm)
		realtext = text + "\n\nHit Enter to confirm, Esc to cancel";
	else
		realtext = text + "\n\nHit Enter or Esc to dismiss";
	unsigned extent_w = 0;
	unsigned extent_h = 0;
	int32_t pos_x = 0;
	int32_t pos_y = 0;
	auto s2 = decode_utf8(realtext);
	for(auto i : s2) {
		auto g = find_glyph(i, pos_x, pos_y, 0, pos_x, pos_y);
		if(pos_x + g.first > extent_w)
			extent_w = static_cast<uint32_t>(pos_x + g.first);
		if(pos_y + 16 > static_cast<int32_t>(extent_h))
			extent_h = static_cast<uint32_t>(pos_y + 16);
	}
	uint32_t x1;
	uint32_t x2;
	uint32_t y1;
	uint32_t y2;
	if(extent_w + 12 >= static_cast<uint32_t>(surf->w)) {
		x1 = 6;
		x2 = surf->w - 6;
		extent_w = x2 - x1;
	} else {
		x1 = (surf->w - extent_w) / 2;
		x2 = x1 + extent_w;
	}
	if(extent_h + 12 >= static_cast<uint32_t>(surf->h)) {
		y1 = 6;
		y2 = surf->h - 6;
		extent_h = y2 - y1;
	} else {
		y1 = (surf->h - extent_h) / 2;
		y2 = y1 + extent_h;
	}
	uint32_t color = surf->format->Rmask | ((surf->format->Gmask >> 1) & surf->format->Gmask);
	draw_box(surf, x1, y1, extent_w, extent_h, color);
	draw_string(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, surf->format->BytesPerPixel, s2,
		x1, y1, extent_w, color);
}

void sdlw_paint_modal_dialog(const std::string& text, bool confirm)
{
	modal_dialog_shown = true;
	modal_dialog_text = text;
	modal_dialog_confirm = confirm;
	SDL_LockSurface(hwsurf);
	paint_modal_dialog(hwsurf, text, confirm);
	SDL_UnlockSurface(hwsurf);
	SDL_UpdateRect(hwsurf, 0, 0, 0, 0);
}

void sdlw_clear_modal_dialog()
{
	modal_dialog_shown = false;
}


namespace
{
	class painter_listener : public information_dispatch
	{
	public:
		painter_listener();
		void on_screen_resize(screen& scr, uint32_t w, uint32_t h);
		void on_render_update_start();
		void on_render_update_end();
		void on_status_update();
	} plistener;

	painter_listener::painter_listener() : information_dispatch("SDL-painter-listener") {}

	void painter_listener::on_screen_resize(screen& scr, uint32_t w, uint32_t h)
	{
		if(w != dp.real_screen_w || h != dp.real_screen_h || !hwsurf ||
			fullscreen_console_active != dp.fullscreen_console) {
			dp = sdlw_display_parameters(w, h, fullscreen_console_active);
			SDL_Surface* hwsurf2 = SDL_SetVideoMode(dp.display_w, dp.display_h, 32, SDL_SWSURFACE);
			if(!hwsurf2) {
				//We are in too fucked up state to even print error as message.
				std::cout << "PANIC: Can't create/resize window: " << SDL_GetError() << std::endl;
				exit(1);
			}
			hwsurf = hwsurf2;
		}
		scr.set_palette(hwsurf->format->Rshift, hwsurf->format->Gshift, hwsurf->format->Bshift);
		if(fullscreen_console_active)
			//We have to render to memory buffer.
			scr.reallocate(w, h, false);
		else {
			//Render direct to screen. But delay setting the buffer.
		}
		our_screen = &scr;
	}

	void painter_listener::on_render_update_start()
	{
		if(fullscreen_console_active)
			return;
		SDL_LockSurface(hwsurf);
		video_locked = true;
		our_screen->set(reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(hwsurf->pixels) +
			dp.screenarea_y * hwsurf->pitch + dp.screenarea_y * hwsurf->format->BytesPerPixel),
			dp.real_screen_w, dp.real_screen_h, hwsurf->pitch);
		our_screen->set_palette(hwsurf->format->Rshift, hwsurf->format->Gshift, hwsurf->format->Bshift);
	}

	void painter_listener::on_render_update_end()
	{
		if(!video_locked)
			return;
		SDL_UnlockSurface(hwsurf);
		video_locked = false;
		if(screen_paintable) {
			SDL_UpdateRect(hwsurf, 6, 6, dp.real_screen_w, dp.real_screen_h);
			screen_paintable = false;
		} else
			screen_dirty = true;
		have_received_frames = true;
	}

	void painter_listener::on_status_update()
	{
		SDL_LockSurface(hwsurf);
		bool update = paint_status(hwsurf, dp, false);
		SDL_UnlockSurface(hwsurf);
		if(update)
			SDL_UpdateRect(hwsurf, dp.statusarea_x, dp.statusarea_y, dp.statusarea_w, dp.statusarea_h);
	}
}

void sdlw_command_updated()
{
	SDL_LockSurface(hwsurf);
	bool update = paint_command(hwsurf, dp, false);
	SDL_UnlockSurface(hwsurf);
	if(update)
		SDL_UpdateRect(hwsurf, dp.cmdline_x, dp.cmdline_y, dp.cmdline_w, 16);
}

void sdlw_screen_paintable()
{
	if(screen_dirty) {
		SDL_UpdateRect(hwsurf, 6, 6, dp.real_screen_w, dp.real_screen_h);
		screen_dirty = false;
	} else
		screen_paintable = true;
}

void window::notify_message() throw(std::bad_alloc, std::runtime_error)
{
	SDL_LockSurface(hwsurf);
	bool update = paint_messages(hwsurf, dp, false);
	SDL_UnlockSurface(hwsurf);
	if(update)
		SDL_UpdateRect(hwsurf, dp.messagearea_x, dp.messagearea_y, dp.messagearea_w, dp.messagearea_h);
}

void sdlw_fullscreen_console(bool enable)
{
	fullscreen_console_active = enable;
	window::msgbuf.set_max_window_size(dp.messagearea_lines);
	sdlw_force_paint();
}

void sdlw_force_paint()
{
	if(!hwsurf) {
		//Initialize video.
		dp = sdlw_display_parameters(0, 0, fullscreen_console_active);
		SDL_Surface* hwsurf2 = SDL_SetVideoMode(dp.display_w, dp.display_h, 32, SDL_SWSURFACE);
		if(!hwsurf2) {
			//We are in too fucked up state to even print error as message.
			std::cout << "PANIC: Can't create/resize window: " << SDL_GetError() << std::endl;
			exit(1);
		}
		hwsurf = hwsurf2;
	}
	SDL_LockSurface(hwsurf);
	paint_screen(hwsurf, dp, true);
	paint_status(hwsurf, dp, true);
	paint_messages(hwsurf, dp, true);
	paint_command(hwsurf, dp, true);
	SDL_UnlockSurface(hwsurf);
	SDL_UpdateRect(hwsurf, 0, 0, 0, 0);
	if(have_received_frames)
		redraw_framebuffer();
	if(modal_dialog_shown) {
		SDL_LockSurface(hwsurf);
		paint_modal_dialog(hwsurf, modal_dialog_text, modal_dialog_confirm);
		SDL_UnlockSurface(hwsurf);
		SDL_UpdateRect(hwsurf, 0, 0, 0, 0);
	}
}

std::string sdlw_decode_string(std::string e)
{
	std::string x;
	for(size_t i = 0; i < e.length(); i += 4) {
		char tmp[5] = {0};
		uint32_t c1 = e[i] - 33;
		uint32_t c2 = e[i + 1] - 33;
		uint32_t c3 = e[i + 2] - 33;
		uint32_t c4 = e[i + 3] - 33;
		uint32_t c = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
		if(c < 0x80) {
			tmp[0] = c;
		} else if(c < 0x800) {
			tmp[0] = 0xC0 | (c >> 6);
			tmp[1] = 0x80 | (c & 0x3F);
		} else if(c < 0x10000) {
			tmp[0] = 0xE0 | (c >> 12);
			tmp[1] = 0x80 | ((c >> 6) & 0x3F);
			tmp[2] = 0x80 | (c & 0x3F);
		} else {
			tmp[0] = 0xF0 | (c >> 18);
			tmp[1] = 0x80 | ((c >> 12) & 0x3F);
			tmp[2] = 0x80 | ((c >> 6) & 0x3F);
			tmp[3] = 0x80 | (c & 0x3F);
		}
		x = x + tmp;
	}
	return x;
}
