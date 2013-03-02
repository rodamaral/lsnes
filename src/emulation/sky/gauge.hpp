#ifndef _skycore__gauge__hpp__included__
#define _skycore__gauge__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <vector>
#include "util.hpp"

namespace sky
{
	struct gauge
	{
		gauge();
		gauge(const std::vector<char>& dispdat, size_t cells);
		uint16_t get_position(size_t idx)
		{
			return (idx < ptr.size()) ? combine(data[ptr[idx]], data[ptr[idx] + 1]) : 0;
		}
		uint8_t* get_data(size_t idx) { return (idx < ptr.size()) ? &data[ptr[idx]] + 2 : dummyimage; }
		size_t maxlimit() { return ptr.size(); }
	private:
		void unpack_image(const std::vector<char>& dispdat, size_t sequence, size_t total);
		std::vector<uint8_t> data;
		std::vector<size_t> ptr;
		uint8_t dummyimage[2];
	};
}
#endif
