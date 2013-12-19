#include "pixfmt-rgb32.hpp"

pixel_format_rgb32::~pixel_format_rgb32() throw() {}

void pixel_format_rgb32::decode(uint32_t* target, const uint8_t* src, size_t width) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = _src[i];
}

void pixel_format_rgb32::decode(uint32_t* target, const uint8_t* src, size_t width,
	const framebuffer::auxpalette<false>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++) {
		target[i] = ((_src[i] >> 16) & 0xFF) << auxp.rshift;
		target[i] |= ((_src[i] >> 8) & 0xFF) << auxp.gshift;
		target[i] |= (_src[i] & 0xFF) << auxp.bshift;
	}
}

void pixel_format_rgb32::decode(uint64_t* target, const uint8_t* src, size_t width,
	const framebuffer::auxpalette<true>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++) {
		target[i] = static_cast<uint64_t>((_src[i] >> 16) & 0xFF) << auxp.rshift;
		target[i] |= static_cast<uint64_t>((_src[i] >> 8) & 0xFF) << auxp.gshift;
		target[i] |= static_cast<uint64_t>(_src[i] & 0xFF) << auxp.bshift;
		target[i] += (target[i] << 8);
	}
}

void pixel_format_rgb32::set_palette(framebuffer::auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

void pixel_format_rgb32::set_palette(framebuffer::auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

uint8_t pixel_format_rgb32::get_bpp() throw()
{
	return 4;
}

uint8_t pixel_format_rgb32::get_ss_bpp() throw()
{
	return 3;
}

uint32_t pixel_format_rgb32::get_magic() throw()
{
	return 0x74212536U;
}

pixel_format_rgb32 _pixel_format_rgb32;
