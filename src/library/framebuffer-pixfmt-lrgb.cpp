#include "framebuffer-pixfmt-lrgb.hpp"

namespace framebuffer
{
_pixfmt_lrgb::~_pixfmt_lrgb() throw()
{
}

void _pixfmt_lrgb::decode(uint32_t* target, const uint8_t* src, size_t width)
	throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++) {
		uint32_t word = _src[i];
		uint32_t l = 1 + ((word >> 15) & 0xF);
		uint32_t r = l * ((word >> 0) & 0x1F);
		uint32_t g = l * ((word >> 5) & 0x1F);
		uint32_t b = l * ((word >> 10) & 0x1F);
		uint32_t x = (((r << 8) - r + 248) / 496) << 16;
		x |= (((g << 8) - g + 248) / 496) << 8;
		x |= ((b << 8) - b + 248) / 496;
		target[i] = x;
	}
}

void _pixfmt_lrgb::decode(uint32_t* target, const uint8_t* src, size_t width,
	const auxpalette<false>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = auxp.pcache[_src[i] & 0x7FFFF];
}

void _pixfmt_lrgb::decode(uint64_t* target, const uint8_t* src, size_t width,
	const auxpalette<true>& auxp) throw()
{
	const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = auxp.pcache[_src[i] & 0x7FFFF];
}

void _pixfmt_lrgb::set_palette(auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.pcache.resize(0x80000);
	for(size_t i = 0; i < 0x80000; i++) {
		uint32_t l = 1 + ((i >> 15) & 0xF);
		uint32_t r = l * ((i >> 0) & 0x1F);
		uint32_t g = l * ((i >> 5) & 0x1F);
		uint32_t b = l * ((i >> 10) & 0x1F);
		auxp.pcache[i] = (((r << 8) - r + 248) / 496) << rshift;
		auxp.pcache[i] += (((g << 8) - g + 248) / 496) << gshift;
		auxp.pcache[i] += (((b << 8) - b + 248) / 496) << bshift;
	}
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
}

void _pixfmt_lrgb::set_palette(auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.pcache.resize(0x80000);
	for(size_t i = 0; i < 0x80000; i++) {
		uint64_t l = 1 + ((i >> 15) & 0xF);
		uint64_t r = l * ((i >> 0) & 0x1F);
		uint64_t g = l * ((i >> 5) & 0x1F);
		uint64_t b = l * ((i >> 10) & 0x1F);
		auxp.pcache[i] = (((r << 16) - r + 248) / 496) << rshift;
		auxp.pcache[i] += (((g << 16) - g + 248) / 496) << gshift;
		auxp.pcache[i] += (((b << 16) - b + 248) / 496) << bshift;
	}
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
}

uint8_t _pixfmt_lrgb::get_bpp() throw()
{
	return 4;
}

uint8_t _pixfmt_lrgb::get_ss_bpp() throw()
{
	return 3;
}

uint32_t _pixfmt_lrgb::get_magic() throw()
{
	return 0;
}

_pixfmt_lrgb pixfmt_lrgb;
}
