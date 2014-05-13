#include "framebuffer-pixfmt-rgb15.hpp"
#include "framebuffer.hpp"

namespace framebuffer
{
template<bool uvswap>
_pixfmt_rgb15<uvswap>::~_pixfmt_rgb15() throw()
{
}

template<bool uvswap>
void _pixfmt_rgb15<uvswap>::decode(uint32_t* target, const uint8_t* src, size_t width)
	throw()
{
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < width; i++) {
		uint32_t word = _src[i];
		uint64_t r = ((word >> (uvswap ? 10 : 0)) & 0x1F);
		uint64_t g = ((word >> 5) & 0x1F);
		uint64_t b = ((word >> (uvswap ? 0 : 10)) & 0x1F);
		target[i] = (((r << 8) - r + 15) / 31) << 16;
		target[i] |= (((g << 8) - g + 15) / 31) << 8;
		target[i] |= (((b << 8) - b + 15) / 31);
	}
}

template<bool uvswap>
void _pixfmt_rgb15<uvswap>::decode(uint32_t* target, const uint8_t* src, size_t width,
	const auxpalette<false>& auxp) throw()
{
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = auxp.pcache[_src[i] & 0x7FFF];
}

template<bool uvswap>
void _pixfmt_rgb15<uvswap>::decode(uint64_t* target, const uint8_t* src, size_t width,
	const auxpalette<true>& auxp) throw()
{
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < width; i++)
		target[i] = auxp.pcache[_src[i] & 0x7FFF];
}

template<bool uvswap>
void _pixfmt_rgb15<uvswap>::set_palette(auxpalette<false>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.pcache.resize(0x8000);
	for(size_t i = 0; i < 0x8000; i++) {
		uint32_t r = ((i >> (uvswap ? 10 : 0)) & 0x1F);
		uint32_t g = ((i >> 5) & 0x1F);
		uint32_t b = ((i >> (uvswap ? 0 : 10)) & 0x1F);
		auxp.pcache[i] = (((r << 8) - r + 15) / 31) << rshift;
		auxp.pcache[i] += (((g << 8) - g + 15) / 31) << gshift;
		auxp.pcache[i] += (((b << 8) - b + 15) / 31) << bshift;
	}
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
}

template<bool uvswap>
void _pixfmt_rgb15<uvswap>::set_palette(auxpalette<true>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.pcache.resize(0x8000);
	for(size_t i = 0; i < 0x8000; i++) {
		uint64_t r = ((i >> (uvswap ? 10 : 0)) & 0x1F);
		uint64_t g = ((i >> 5) & 0x1F);
		uint64_t b = ((i >> (uvswap ? 0 : 10)) & 0x1F);
		auxp.pcache[i] = (((r << 16) - r + 15) / 31) << rshift;
		auxp.pcache[i] += (((g << 16) - g + 15) / 31) << gshift;
		auxp.pcache[i] += (((b << 16) - b + 15) / 31) << bshift;
	}
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
}

template<bool uvswap>
uint8_t _pixfmt_rgb15<uvswap>::get_bpp() throw()
{
	return 2;
}

template<bool uvswap>
uint8_t _pixfmt_rgb15<uvswap>::get_ss_bpp() throw()
{
	return 2;
}

template<bool uvswap>
uint32_t _pixfmt_rgb15<uvswap>::get_magic() throw()
{
	if(uvswap)
		return 0x16326322;
	else
		return 0x74324563;
}

_pixfmt_rgb15<false> pixfmt_rgb15;
_pixfmt_rgb15<true> pixfmt_bgr15;
}
