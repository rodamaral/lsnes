#include "util.hpp"

namespace sky
{
	vector_input_stream::vector_input_stream(const std::vector<char>& _data, size_t _offset)
		: data(_data), offset(_offset)
	{
	}

	vector_input_stream::~vector_input_stream()
	{
	}

	int vector_input_stream::get()
	{
		if(offset < data.size())
			return (uint8_t)data[offset++];
		return -1;
	}
}
