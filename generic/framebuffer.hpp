#ifndef _framebuffer__hpp__included__
#define _framebuffer__hpp__included__

#include "render.hpp"

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
 * Redraw the framebuffer on screen.
 */
void redraw_framebuffer();

#endif