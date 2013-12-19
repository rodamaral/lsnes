#include "framebuffer-pixfmt-rgb32.hpp"

namespace framebuffer
{
_pixfmt_rgb32::~_pixfmt_rgb32() throw() {}

void _pixfmt_rgb32::decode(uint32_t* target, const uint8_t* src, size_t width) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = _src[i];
}

void _pixfmt_rgb32::decode(uint32_t* target, const uint8_t* src, size_t width,
	const auxpalette<false>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++) {
		target[i] = ((_src[i] >> 16) & 0xFF) << auxp.rshift;
		target[i] |= ((_src[i] >> 8) & 0xFF) << auxp.gshift;
		target[i] |= (_src[i] & 0xFF) << auxp.bshift;
	}
}

void _pixfmt_rgb32::decode(uint64_t* target, const uint8_t* src, size_t width,
	const auxpalette<true>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++) {
		target[i] = static_cast<uint64_t>((_src[i] >> 16) & 0xFF) << auxp.rshift;
		target[i] |= static_cast<uint64_t>((_src[i] >> 8) & 0xFF) << auxp.gshift;
		target[i] |= static_cast<uint64_t>(_src[i] & 0xFF) << auxp.bshift;
		target[i] += (target[i] << 8);
	}
}

void _pixfmt_rgb32::set_palette(auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

void _pixfmt_rgb32::set_palette(auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

uint8_t _pixfmt_rgb32::get_bpp() throw()
{
	return 4;
}

uint8_t _pixfmt_rgb32::get_ss_bpp() throw()
{
	return 3;
}

uint32_t _pixfmt_rgb32::get_magic() throw()
{
	return 0x74212536U;
}

_pixfmt_rgb32 pixfmt_rgb32;
}
