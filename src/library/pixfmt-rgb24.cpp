#include "pixfmt-rgb24.hpp"
#include <cstring>

template<bool uvswap>
pixel_format_rgb24<uvswap>::~pixel_format_rgb24() throw() {}

template<bool uvswap>
void pixel_format_rgb24<uvswap>::decode(uint32_t* target, const uint8_t* src, size_t width) throw()
{
	if(uvswap) {
		for(size_t i = 0; i < width; i++) {
			target[i] = (uint32_t)src[3 * i + 2] << 16;
			target[i] |= (uint32_t)src[3 * i + 1] << 8;
			target[i] |= src[3 * i + 0];
		}
	} else {
		for(size_t i = 0; i < width; i++) {
			target[i] = (uint32_t)src[3 * i + 0] << 16;
			target[i] |= (uint32_t)src[3 * i + 1] << 8;
			target[i] |= src[3 * i + 2];
		}
	}
}

template<bool uvswap>
void pixel_format_rgb24<uvswap>::decode(uint32_t* target, const uint8_t* src, size_t width,
	const pixel_format_aux_palette<false>& auxp) throw()
{
	for(size_t i = 0; i < width; i++) {
		target[i] = static_cast<uint32_t>(src[3 * i + (uvswap ? 2 : 0)]) << auxp.rshift;
		target[i] |= static_cast<uint32_t>(src[3 * i + 1]) << auxp.gshift;
		target[i] |= static_cast<uint32_t>(src[3 * i + (uvswap ? 0 : 2)]) << auxp.bshift;
	}
}

template<bool uvswap>
void pixel_format_rgb24<uvswap>::decode(uint64_t* target, const uint8_t* src, size_t width,
	const pixel_format_aux_palette<true>& auxp) throw()
{
	for(size_t i = 0; i < width; i++) {
		target[i] = static_cast<uint64_t>(src[3 * i + (uvswap ? 2 : 0)]) << auxp.rshift;
		target[i] |= static_cast<uint64_t>(src[3 * i + 1]) << auxp.gshift;
		target[i] |= static_cast<uint64_t>(src[3 * i + (uvswap ? 0 : 2)]) << auxp.bshift;
		target[i] += (target[i] << 8);
	}
}

template<bool uvswap>
void pixel_format_rgb24<uvswap>::set_palette(pixel_format_aux_palette<false>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

template<bool uvswap>
void pixel_format_rgb24<uvswap>::set_palette(pixel_format_aux_palette<true>& auxp, uint8_t rshift, uint8_t gshift,
	uint8_t bshift) throw(std::bad_alloc)
{
	auxp.rshift = rshift;
	auxp.gshift = gshift;
	auxp.bshift = bshift;
	auxp.pcache.clear();
}

template<bool uvswap>
uint8_t pixel_format_rgb24<uvswap>::get_bpp() throw()
{
	return 3;
}

template<bool uvswap>
uint8_t pixel_format_rgb24<uvswap>::get_ss_bpp() throw()
{
	return 3;
}

template<bool uvswap>
uint32_t pixel_format_rgb24<uvswap>::get_magic() throw()
{
	if(uvswap)
		return 0x25642332U;
	else
		return 0x85433684U;
}

pixel_format_rgb24<false> _pixel_format_rgb24;
pixel_format_rgb24<true> _pixel_format_bgr24;
