#ifndef _framebuffer__hpp__included__
#define _framebuffer__hpp__included__

#include "core/window.hpp"
#include "library/framebuffer.hpp"
#include "library/triplebuffer.hpp"

#include <stdexcept>

class subtitle_commentary;
class memwatch_set;
class emulator_dispatch;
namespace settingvar
{
	class group;
}
namespace keyboard
{
	class keyboard;
}

/**
 * Emulator frame buffer.
 */
class emu_framebuffer
{
public:
	emu_framebuffer(subtitle_commentary& _subtitles, settingvar::group& _settings, memwatch_set& _mwatch,
		keyboard::keyboard& _keyboard, emulator_dispatch& _dispatch);
/**
 * The main framebuffer.
 */
	framebuffer::raw main_framebuffer;
/**
 * Special screen: "SYSTEM STATE CORRUPT".
 */
	static framebuffer::raw screen_corrupt;
/**
 * The main screen to draw on.
 */
	framebuffer::fb<false> main_screen;
/**
 * Initialize special screens.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void init_special_screens() throw(std::bad_alloc);
/**
 * Copy framebuffer to backing store, running Lua hooks if any.
 */
	void redraw_framebuffer(framebuffer::raw& torender, bool no_lua = false, bool spontaneous = false);
/**
 * Redraw the framebuffer, reusing contents from last redraw. Runs lua hooks if last redraw ran them.
 */
	void redraw_framebuffer();
/**
 * Return last complete framebuffer.
 */
	framebuffer::raw get_framebuffer() throw(std::bad_alloc);
/**
 * Render framebuffer to main screen.
 */
	void render_framebuffer();
/**
 * Get the size of current framebuffer.
 */
	std::pair<uint32_t, uint32_t> get_framebuffer_size();
/**
 * Take a screenshot to specified file.
 */
	void take_screenshot(const std::string& file) throw(std::bad_alloc, std::runtime_error);
/**
 * Kill pending requests associated with object.
 */
	void render_kill_request(void* obj);
/**
 * Get latest screen received from core.
 */
	framebuffer::raw& render_get_latest_screen();
	void render_get_latest_screen_end();
private:
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
	render_info buffer1;
	render_info buffer2;
	render_info buffer3;
	triplebuffer::triplebuffer<render_info> buffering;
	bool last_redraw_no_lua;
	subtitle_commentary& subtitles;
	settingvar::group& settings;
	memwatch_set& mwatch;
	keyboard::keyboard& keyboard;
	emulator_dispatch& edispatch;
};

#endif
