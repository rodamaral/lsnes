#ifndef _framebuffer__hpp__included__
#define _framebuffer__hpp__included__

#include "render.hpp"
#include "window.hpp"

extern lcscreen framebuffer;
extern lcscreen screen_nosignal;
extern lcscreen screen_corrupt;
extern screen main_screen;
void init_special_screens() throw(std::bad_alloc);
void redraw_framebuffer(window* win);

#endif