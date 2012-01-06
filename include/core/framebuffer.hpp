#ifndef _framebuffer__hpp__included__
#define _framebuffer__hpp__included__

#include "core/render.hpp"

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
extern screen main_screen;
/**
 * Initialize special screens.
 *
 * throws std::bad_alloc: Not enough memory.
 */
void init_special_screens() throw(std::bad_alloc);
/**
 * Copy framebuffer to backing store, running Lua hooks if any.
 */
void redraw_framebuffer(lcscreen& torender, bool no_lua = false);
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