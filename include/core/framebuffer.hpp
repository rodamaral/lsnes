#ifndef _framebuffer__hpp__included__
#define _framebuffer__hpp__included__

#include "core/render.hpp"
#include "core/window.hpp"

#include <stdexcept>

/**
 * Triple buffering logic.
 */
class triplebuffer_logic
{
public:
/**
 * Create new triple buffer logic.
 */
	triplebuffer_logic() throw(std::bad_alloc);
/**
 * Destructor.
 */
	~triplebuffer_logic() throw();
/**
 * Get index for write buffer.Also starts write cycle.
 *
 * Returns: Write buffer index (0-2).
 */
	unsigned start_write() throw();
/**
 * Notify that write cycle has ended.
 */
	void end_write() throw();
/**
 * Get index for read buffer. Also starts read cycle.
 *
 * Returns: Read buffer index (0-2).
 */
	unsigned start_read() throw();
/**
 * Notify that read cycle has ended.
 */
	void end_read() throw();
private:
	triplebuffer_logic(triplebuffer_logic&);
	triplebuffer_logic& operator=(triplebuffer_logic&);
	mutex* mut;
	unsigned read_active;
	unsigned write_active;
	unsigned read_active_slot;
	unsigned write_active_slot;
	unsigned last_complete_slot;
};

/**
 * The main framebuffer.
 */
extern lcscreen framebuffer;
/**
 * Special screen: "NO SIGNAL".
 */
extern lcscreen screen_nosignal;
/**
 * Special screen: "SYSTEM STATE CORRUPT".
 */
extern lcscreen screen_corrupt;
/**
 * The main screen to draw on.
 */
extern screen<false> main_screen;
/**
 * Initialize special screens.
 *
 * throws std::bad_alloc: Not enough memory.
 */
void init_special_screens() throw(std::bad_alloc);
/**
 * Copy framebuffer to backing store, running Lua hooks if any.
 */
void redraw_framebuffer(lcscreen& torender, bool no_lua = false, bool spontaneous = false);
/**
 * Redraw the framebuffer, reusing contents from last redraw. Runs lua hooks if last redraw ran them.
 */
void redraw_framebuffer();
/**
 * Return last complete framebuffer.
 */
lcscreen get_framebuffer() throw(std::bad_alloc);
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
 * Get scale factors.
 */
std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height);

#endif
