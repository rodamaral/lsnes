#ifndef _skycore__lzs__hpp__included__
#define _skycore__lzs__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace sky
{
	struct data_error : public std::runtime_error
	{
		data_error(const char* errmsg);
	};

	struct input_stream
	{
		virtual ~input_stream();
		virtual int get() = 0;
		unsigned char read(const char* msg)
		{
			int r = get();
			if(r < 0) throw data_error(msg);
			return (r & 0xFF);
		}
	};

	struct output_stream
	{
		virtual void put(unsigned char byte) = 0;
	};

	void lzs_decompress(input_stream& in, output_stream& out, size_t size);
}
#endif
