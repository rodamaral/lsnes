#ifndef _skycore__image__hpp__included__
#define _skycore__image__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace sky
{
	struct image
	{
		image();
		image(const std::vector<char>& data);
		uint16_t width;
		uint16_t height;
		unsigned colors;
		uint16_t unknown1;
		std::vector<uint8_t> decode;
		uint32_t operator[](size_t ptr) { return palette[decode[ptr]]; }
		uint32_t palette[256];
		uint8_t unknown2[512];
	};
}

#endif
