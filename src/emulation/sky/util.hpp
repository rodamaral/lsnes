#ifndef _skycore__util__hpp__included__
#define _skycore__util__hpp__included__

#include <cstdint>
#include <vector>
#include "lzs.hpp"

namespace sky
{
	inline uint8_t expand_color(uint8_t vgacolor)
	{
		vgacolor &= 0x3F;
		return (vgacolor << 2) | (vgacolor >> 4);
	}

	inline uint8_t access_array(const std::vector<char>& data, size_t offset)
	{
		if(offset >= data.size())
			throw std::runtime_error("Attempt to access array outside bounds");
		return data[offset];
	}

	inline uint16_t combine(uint8_t a, uint8_t b)
	{
		return ((uint16_t)b << 8) | (uint16_t)a;
	}

	inline int sgn(int16_t v)
	{
		if(v < 0)
			return -1;
		if(v > 0)
			return 1;
		return 0;
	}

	inline int sgn(int32_t v)
	{
		if(v < 0)
			return -1;
		if(v > 0)
			return 1;
		return 0;
	}

	struct vector_input_stream : public input_stream
	{
		vector_input_stream(const std::vector<char>& _data, size_t _offset);
		~vector_input_stream();
		int get();
	private:
		const std::vector<char>& data;
		size_t offset;
	};
}
#endif
