#ifndef _plat_sdl__paint__hpp__included__
#define _plat_sdl__paint__hpp__included__

#include <cstdint>
#include <string>

#include <SDL.h>

struct sdlw_display_parameters
{
	//Fill this structure.
	sdlw_display_parameters();
	sdlw_display_parameters(uint32_t rscrw, uint32_t rscrh, bool cactive);
	//Real width of screen buffer.
	uint32_t real_screen_w;
	//Real height of screen buffer.
	uint32_t real_screen_h;
	//Fullscreen console active flag.
	bool fullscreen_console;
	//Virtual width of screen area.
	uint32_t virtual_screen_w;
	//Virtual height of screen area.
	uint32_t virtual_screen_h;
	//Display width.
	uint32_t display_w;
	//Display height.
	uint32_t display_h;
	//Screen area left edge x.
	uint32_t screenarea_x;
	//Screen area top edge y.
	uint32_t screenarea_y;
	//Screen area width.
	uint32_t screenarea_w;
	//Screen area height.
	uint32_t screenarea_h;
	//Status area left edge x.
	uint32_t statusarea_x;
	//Status area top edge y.
	uint32_t statusarea_y;
	//Status area width.
	uint32_t statusarea_w;
	//Status area height.
	uint32_t statusarea_h;
	//Status area lines.
	uint32_t statusarea_lines;
	//Message area left edge x.
	uint32_t messagearea_x;
	//Message area top edge y.
	uint32_t messagearea_y;
	//Message area width.
	uint32_t messagearea_w;
	//Message area height.
	uint32_t messagearea_h;
	//Message area lines.
	uint32_t messagearea_lines;
	//Message area trailing blank top edge y.
	uint32_t messagearea_trailing_y;
	//Message area trailing blank height.
	uint32_t messagearea_trailing_h;
	//Command line left edge x.
	uint32_t cmdline_x;
	//Command line top edge y.
	uint32_t cmdline_y;
	//Command line width
	uint32_t cmdline_w;
};

struct command_status
{
	bool active;
	bool overwrite;
	uint32_t rawpos;
	std::string encoded;
};

//Query status of command line.
struct command_status get_current_command();

//Draw outline box. The surface must be locked.
void draw_box(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
//Draw main screen. The surface must be locked.
bool paint_screen(SDL_Surface* surf, const sdlw_display_parameters& p, bool full);
//Draw status area. The surface must be locked.
bool paint_status(SDL_Surface* surf, const sdlw_display_parameters& p, bool full);
//Draw messages. The surface must be locked.
bool paint_messages(SDL_Surface* surf, const sdlw_display_parameters& p, bool full);
//Draw command. The surface must be locked.
bool paint_command(SDL_Surface* surf, const sdlw_display_parameters& p, bool full);
//Draw a modal dialog. The surface must be locked.
void paint_modal_dialog(SDL_Surface* surf, const std::string& text, bool confirm);

void sdlw_paint_modal_dialog(const std::string& text, bool confirm);
void sdlw_clear_modal_dialog();
void sdlw_screen_paintable();
void sdlw_command_updated();
void sdlw_fullscreen_console(bool enable);
void sdlw_force_paint();
std::string sdlw_decode_string(std::string e);

#endif
