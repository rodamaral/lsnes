#ifndef _library__framebuffer_pixfmt_rgb24__hpp__included__
#define _library__framebuffer_pixfmt_rgb24__hpp__included__

#include "framebuffer-pixfmt.hpp"

namespace framebuffer
{
/**
 * Pixel format RGB24.
 */
template<bool uvswap>
class _pixfmt_rgb24 : public pixfmt
{
public:
	~_pixfmt_rgb24() throw();
	void decode(uint32_t* target, const uint8_t* src, size_t width)
		throw();
	void decode(uint32_t* target, const uint8_t* src, size_t width,
		const auxpalette<false>& auxp) throw();
	void decode(uint64_t* target, const uint8_t* src, size_t width,
		const auxpalette<true>& auxp) throw();
	void set_palette(auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc);
	void set_palette(auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
		uint8_t bshift) throw(std::bad_alloc);
	uint8_t get_bpp() throw();
	uint8_t get_ss_bpp() throw();
	uint32_t get_magic() throw();
};

extern _pixfmt_rgb24<false> pixfmt_rgb24;
extern _pixfmt_rgb24<true> pixfmt_bgr24;
}

#endif
