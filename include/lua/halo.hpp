#ifndef _lua__halo__hpp__included__
#define _lua__halo__hpp__included__

#include <cstdlib>
#include "library/framebuffer.hpp"

/**
 * Render a 1px wide halo around monochrome image.
 *
 * Parameter pixmap: The pixmap to render halo on. Must be aligned to 32 bytes.
 * Parameter width: Width of the pixmap. Must be multiple of 32 bytes.
 * Parameter height: Height of the pixmap.
 */
void render_halo(unsigned char* pixmap, size_t width, size_t height);

/**
 * Blit a bitmap to screen.
 */
template<bool X> void halo_blit(struct framebuffer::fb<X>& scr, unsigned char* pixmap, size_t width,
	size_t height, size_t owidth, size_t oheight, uint32_t x, uint32_t y, framebuffer::color& bg,
	framebuffer::color& fg, framebuffer::color& hl) throw();

#endif
