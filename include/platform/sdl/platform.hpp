#ifndef _plat_sdl__platform__hpp__included__
#define _plat_sdl__platform__hpp__included__

#include "core/keymapper.hpp"
#include "core/window.hpp"

#include <cstdint>
#include <SDL.h>
#include <stdexcept>
#include <list>
#include <vector>

//No-op
#define SPECIAL_NOOP		0x40000000UL
//Acknowledge line.
#define SPECIAL_ACK		0x40000001UL
//Negative acknowledge line.
#define SPECIAL_NAK		0x40000002UL
//Backspace.
#define SPECIAL_BACKSPACE	0x40000003UL
//Insert
#define SPECIAL_INSERT		0x40000004UL
//Delete
#define SPECIAL_DELETE		0x40000005UL
//Home
#define SPECIAL_HOME		0x40000006UL
//End
#define SPECIAL_END		0x40000007UL
//Page Up.
#define SPECIAL_PGUP		0x40000008UL
//Page Down.
#define SPECIAL_PGDN		0x40000009UL
//Up.
#define SPECIAL_UP		0x4000000AUL
//Down.
#define SPECIAL_DOWN		0x4000000BUL
//Left.
#define SPECIAL_LEFT		0x4000000CUL
//Right.
#define SPECIAL_RIGHT		0x4000000DUL
//Pressed mask.
#define PRESSED_MASK		0x80000000UL

//Wake UI thread: Timer tick.
#define WAKE_UI_TIMER_TICK 0
//Wake UI thread: Identify complete.
#define WAKE_UI_IDENTIFY_COMPLETE 1
//Wake UI thread: Repaint
#define WAKE_UI_REPAINT 2

/**
 * Parse SDL event into commandline edit operation.
 *
 * Parameter e: The SDL event.
 * Parameter enable: If true, do parsing, otherwise return SPECIAL_NOOP.
 * Returns: High bit is 1 if key is pressed, else 0. Low 31 bits are unicode key code or some SPECIAL_* value.
 */
uint32_t get_command_edit_operation(SDL_Event& e, bool enable);

/**
 * Init all keyboard keys.
 */
void init_sdl_keys();

/**
 * Deinit all keyboard keys.
 */
void deinit_sdl_keys();

/**
 * Translate SDL keyboard event.
 *
 * Parameter e: The event to translate.
 * Parameter k: The first translation result is written here.
 * Returns: 0 if event is not valid keyboard event. 1 if k has been filled.
 */
unsigned translate_sdl_key(SDL_Event& e, keypress& k);

/**
 * Translate SDL joystick event.
 *
 * Parameter e: The event to translate.
 * Parameter k: The first translation result is written here.
 * Returns: 0 if event is not valid joystick event, 1 if k has been filled.
 */
unsigned translate_sdl_joystick(SDL_Event& e, keypress& k);

/**
 * Draw a string to framebuffer.
 *
 * Parameter base: The base address of framebuffer.
 * Parameter pitch: Separation between lines in framebuffer in bytes.
 * Parameter pbytes: Bytes per pixel.
 * Parameter s: The string to draw.
 * Parameter x: The x-coordinate to start drawing from.
 * Parameter y: The y-coordinate to start drawing from.
 * Parameter maxwidth: Maximum width to draw. If greater than text width, the remainder is blacked, if less than
 *	text width, the text is truncated.
 * Parameter color: The raw color to draw the text using.
 * Parameter hilite_mode: Hilighting mode: 0 => off, 1 => Underline, 2 => Invert
 * Parameter hilite_pos: The character index to hilight. May be length of string (character after end is hilighted).
 */
void draw_string(uint8_t* base, uint32_t pitch, uint32_t pbytes, std::string s, uint32_t x, uint32_t y,
	uint32_t maxwidth, uint32_t color, uint32_t hilite_mode = 0, uint32_t hilite_pos = 0) throw();

/**
 * Draw a string to SDL surface.
 *
 * Parameter surf: The surface to draw on. Must be locked.
 * Parameter s: The string to draw.
 * Parameter x: The x-coordinate to start drawing from.
 * Parameter y: The y-coordinate to start drawing from.
 * Parameter maxwidth: Maximum width to draw. If greater than text width, the remainder is blacked, if less than
 *	text width, the text is truncated.
 * Parameter color: The raw color to draw the text using.
 * Parameter hilite_mode: Hilighting mode: 0 => off, 1 => Underline, 2 => Invert
 * Parameter hilite_pos: The character index to hilight. May be length of string (character after end is hilighted).
 */
void draw_string(SDL_Surface* surf, std::string s, uint32_t x, uint32_t y, uint32_t maxwidth, uint32_t color,
	uint32_t hilite_mode = 0, uint32_t hilite_pos = 0) throw();

/**
 * Draw a box to framebuffer. The contents of box are cleared.
 *
 * Parameter base: The base address of framebuffer.
 * Parameter pitch: Separation between lines in framebuffer in bytes.
 * Parameter pbytes: Bytes per pixel.
 * Parameter width: The width of framebuffer.
 * Parameter height: The height of framebuffer.
 * Parameter x: The x-coordinate of left edge of inner part of box.
 * Parameter y: The y-coordinate of top edge of inner part of box.
 * Parameter w: The width of box inner part. If 0, the box isn't drawn.
 * Parameter h: The height of box inner part. If 0, the box isn't drawn.
 * Parameter color: The raw color to draw the box outline using.
 */
void draw_box(uint8_t* base, uint32_t pitch, uint32_t pbytes, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, uint32_t color) throw();

/**
 * Draw box to SDL surface. The contents of box are cleared.
 *
 * Parameter surf: The surface to draw on. Must be locked.
 * Parameter x: The x-coordinate of left edge of inner part of box.
 * Parameter y: The y-coordinate of top edge of inner part of box.
 * Parameter w: The width of box inner part. If 0, the box isn't drawn.
 * Parameter h: The height of box inner part. If 0, the box isn't drawn.
 * Parameter color: The raw color to draw the box outline using.
 */
void draw_box(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) throw();

/**
 * Decode UTF-8 string into codepoints.
 */
std::vector<uint32_t> decode_utf8(std::string s);


/**
 * The model for command line.
 */
struct commandline_model
{
/**
 * Create new commandline model.
 */
	commandline_model() throw();
/**
 * Execute key.
 *
 * Returns: If key == SPECIAL_ACK, the entered command, otherwise "".
 */
	std::string key(uint32_t key) throw(std::bad_alloc);
/**
 * Do timer tick (for autorepeat).
 */
	void tick() throw(std::bad_alloc);
/**
 * Read the line.
 */
	std::string read_command() throw(std::bad_alloc);
/**
 * Read the cursor position.
 */
	size_t cursor() throw();
/**
 * Is the command line enabled?
 */
	bool enabled() throw();
/**
 * Is in overwrite mode?
 */
	bool overwriting() throw();
/**
 * Enable command line.
 */
	void enable() throw();
/**
 * Repaint to SDL surface.
 *
 * Parameter surf: The surface to paint on, must be locked.
 * Parameter x: The x-coordinate to start painting from.
 * Parameter y: The y-coordinate to start painting from.
 * Parameter maxwidth: The maximum width.
 * Parameter color: The raw color.
 * Parameter box: If true, also paint a box.
 * Parameter boxcolor: If box is true, this is the raw color to paint the box with.
 */
	void paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t maxwidth, uint32_t color, bool box = false,
		uint32_t boxcolor = 0xFFFFFFFFUL) throw();
private:
	void scroll_history_up();
	void scroll_history_down();
	void handle_cow();
	void delete_codepoint(size_t idx);
	bool enabled_flag;
	uint32_t autorepeating_key;
	int autorepeat_phase;	//0 => Off, 1 => First, 2 => Subsequent.
	uint32_t autorepeat_counter;
	std::vector<uint32_t> saved_codepoints;
	std::vector<uint32_t> codepoints;
	size_t cursor_pos;
	bool overwrite_mode;
	std::list<std::vector<uint32_t>> history;
	std::list<std::vector<uint32_t>>::iterator history_itr;
};

/**
 * Status area model.
 */
struct statusarea_model
{
/**
 * Paint the status area.
 *
 * Parameter surf: The SDL surface. Must be locked.
 * Parameter x: Top-left corner x coordinate.
 * Parameter y: Top-left corner y coordinate.
 * Parameter w: Width of status area.
 * Parameter h: height of status area.
 * Parameter color: Color of text.
 * Parameter box: If true, draw the bounding box (impiles full redraw).
 * Parameter boxcolor: The color of surrounding box.
 */
	void paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, bool box = false,
		uint32_t boxcolor = 0xFFFFFFFFUL) throw();
private:
	std::map<std::string, std::string> current_contents;
};

/**
 * Messages area model.
 */
struct messages_model
{
/**
 * Constructor.
 */
	messages_model() throw();
/**
 * Paint the messages area.
 *
 * Parameter surf: The SDL surface. Must be locked.
 * Parameter x: Top-left corner x coordinate.
 * Parameter y: Top-left corner y coordinate.
 * Parameter w: Width of status area.
 * Parameter h: height of status area.
 * Parameter color: Color of text.
 * Parameter box: If true, draw the bounding box (impiles full redraw).
 * Parameter boxcolor: The color of surrounding box.
 */
	void paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, bool box = false,
		uint32_t boxcolor = 0xFFFFFFFFUL) throw();
private:
	uint64_t first_visible;
	uint64_t messages_visible;
};

/**
 * Paint modal message.
 *
 * Parameter surf: The SDL surface. Must be locked.
 * Parameter text: The text for dialog.
 * Parameter confirm: If true, paint as confirm dialog, otherwise as notification dialog.
 * Parameter color: Color of the dialog.
 */
void paint_modal_dialog(SDL_Surface* surf, const std::string& text, bool confirm, uint32_t color) throw();

/**
 * Layout of screen.
 */
struct screen_layout
{
/**
 * Default values.
 */
	screen_layout();
/**
 * Update rest of values from real_width/real_height and fullscreen_console.
 */
	void update() throw();
/**
 * Screen real area.
 */
	uint32_t real_width;
	uint32_t real_height;
/**
 * Fullscreen console mode on.
 */
	bool fullscreen_console;
/**
 * Window.
 */
	uint32_t window_w;
	uint32_t window_h;
/**
 * The screen.
 */
	uint32_t screen_x;
	uint32_t screen_y;
	uint32_t screen_w;
	uint32_t screen_h;
/**
 * Status area.
 */
	uint32_t status_x;
	uint32_t status_y;
	uint32_t status_w;
	uint32_t status_h;
/**
 * Messages area.
 */
	uint32_t messages_x;
	uint32_t messages_y;
	uint32_t messages_w;
	uint32_t messages_h;
/**
 * Command line.
 */
	uint32_t commandline_x;
	uint32_t commandline_y;
	uint32_t commandline_w;
};

/**
 * Model of screen.
 *
 * Don't have multiple of these. The results are undefined. Call all methods from UI thread.
 */
class screen_model
{
public:
/**
 * Constructor.
 */
	screen_model();
/**
 * Do full repaint.
 */
	void repaint_full() throw();
/**
 * Repaint main screen.
 */
	void repaint_screen() throw();
/**
 * Repaint status area.
 */
	void repaint_status() throw();
/**
 * Repaint message area.
 */
	void repaint_messages() throw();
/**
 * Repaint command line.
 */
	void repaint_commandline() throw();
/**
 * Set the command line model to use.
 */
	void set_command_line(commandline_model* c) throw();
/**
 * Set modal dialog.
 */
	void set_modal(const std::string& text, bool confirm) throw();
/**
 * Clear modal dialog.
 */
	void clear_modal() throw();
/**
 * Set fullscreen console mode on/off
 */
	void set_fullscreen_console(bool enable) throw();
/**
 * Do SDL_Flip or equivalent.
 */
	void flip() throw();
/**
 * Temporary palette values.
 */
	unsigned pal_r;
	unsigned pal_g;
	unsigned pal_b;
private:
	void _repaint_screen() throw();
	void repaint_modal() throw();
	SDL_Surface* surf;
	commandline_model* cmdline;
	bool modal_active;
	std::string modal_text;
	bool modal_confirm;
	statusarea_model statusarea;
	messages_model smessages;
	screen_layout layout;
	uint32_t old_screen_w;
	uint32_t old_screen_h;
};

/**
 * Notify the UI code that emulator thread is about to exit (call in the emulator thread).
 */
void notify_emulator_exit();
/**
 * The user interface loop. Call in UI thread. notify_emulator_exit() causes this to return.
 */
void ui_loop();


#endif
