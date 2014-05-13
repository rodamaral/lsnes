#ifndef _library__framebuffer_pixfmt__hpp__included__
#define _library__framebuffer_pixfmt__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace framebuffer
{
template<bool X> class auxpalette;

/**
 * Pixel format.
 */
class pixfmt
{
public:
	virtual ~pixfmt() throw();
/**
 * Register the pixel format.
 */
	pixfmt() throw(std::bad_alloc);
/**
 * Decode pixel format data into RGB data (0, R, G, B).
 */
	virtual void decode(uint32_t* target, const uint8_t* src, size_t width)
		throw() = 0;
/**
 * Decode pixel format data into RGB (with specified byte order).
 */
	virtual void decode(uint32_t* target, const uint8_t* src, size_t width,
		const auxpalette<false>& auxp) throw() = 0;
/**
 * Decode pixel format data into RGB (with specified byte order).
 */
	virtual void decode(uint64_t* target, const uint8_t* src, size_t width,
		const auxpalette<true>& auxp) throw() = 0;
/**
 * Create aux palette.
 */
	virtual void set_palette(auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc) = 0;
/**
 * Create aux palette.
 */
	virtual void set_palette(auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc) = 0;
/**
 * Bytes per pixel in data.
 */
	virtual uint8_t get_bpp() throw() = 0;
/**
 * Bytes per pixel in ss data.
 */
	virtual uint8_t get_ss_bpp() throw() = 0;
/**
 * Screenshot magic (0 for the old format).
 */
	virtual uint32_t get_magic() throw() = 0;
};
}

#endif
