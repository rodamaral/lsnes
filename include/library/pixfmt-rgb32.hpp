#ifndef _library__pixfmt_rgb32__hpp__included__
#define _library__pixfmt_rgb32__hpp__included__

#include "framebuffer.hpp"

/**
 * Pixel format RGB32.
 */
class pixel_format_rgb32 : public pixel_format
{
public:
	~pixel_format_rgb32() throw();
	void decode(uint8_t* target, const uint8_t* src, size_t width)
		throw();
	void decode(uint32_t* target, const uint8_t* src, size_t width,
		const pixel_format_aux_palette<false>& auxp) throw();
	void decode(uint64_t* target, const uint8_t* src, size_t width,
		const pixel_format_aux_palette<true>& auxp) throw();
	void set_palette(pixel_format_aux_palette<false>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc);
	void set_palette(pixel_format_aux_palette<true>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc);
	uint8_t get_bpp() throw();
	uint8_t get_ss_bpp() throw();
	uint32_t get_magic() throw();
};

extern pixel_format_rgb32 _pixel_format_rgb32;

#endif
