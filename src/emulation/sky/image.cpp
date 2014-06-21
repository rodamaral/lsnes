#include "image.hpp"
#include "util.hpp"

namespace sky
{
	struct vector_output_stream : public output_stream
	{
		vector_output_stream(std::vector<uint8_t>& _data);
		~vector_output_stream();
		void put(uint8_t byte);
	private:
		std::vector<uint8_t>& data;
	};

	vector_output_stream::vector_output_stream(std::vector<uint8_t>& _data)
		: data(_data)
	{
	}

	vector_output_stream::~vector_output_stream()
	{
	}

	void vector_output_stream::put(uint8_t byte)
	{
		data.push_back(byte);
	}

	image::image()
	{
		memset(palette, 0, 256 * sizeof(uint32_t));
		memset(unknown2, 0, 512);
		unknown1 = 0;
		width = 0;
		height = 0;
	}

	image::image(const std::vector<char>& data)
	{
		memset(palette, 0, 256 * sizeof(uint32_t));
		memset(unknown2, 0, 512);
		if(data.size() < 5)
			throw std::runtime_error("Expected CMAP magic, got EOF");
		if(data[0] != 'C' || data[1] != 'M' || data[2] != 'A' || data[3] != 'P')
			throw std::runtime_error("Bad CMAP magic");
		colors = (unsigned char)data[4];
		if(data.size() < 5 * colors + 5)
			throw std::runtime_error("Expected CMAP, got EOF");
		for(unsigned i = 0; i < colors; i++) {
			palette[i] = ((uint32_t)expand_color(data[3 * i + 5]) << 16) |
				((uint32_t)expand_color(data[3 * i + 6]) << 8) |
				((uint32_t)expand_color(data[3 * i + 7]));
			unknown2[2 * i + 0] = data[3 * colors + 2 * i + 5];
			unknown2[2 * i + 1] = data[3 * colors + 2 * i + 6];
		}
		if(data.size() < 5 * colors + 9)
			throw std::runtime_error("Bad PICT magic");
		size_t x = 5 * colors + 5;
		if(data[x + 0] != 'P' || data[x + 1] != 'I' || data[x + 2] != 'C' || data[x + 3] != 'T')
			throw std::runtime_error("Bad PICT magic");
		if(data.size() < 5 * colors + 15)
			throw std::runtime_error("Expected PICT header, got EOF");
		unknown1 = combine(data[x + 4], data[x + 5]);
		height = combine(data[x + 6], data[x + 7]);
		width = combine(data[x + 8], data[x + 9]);
		vector_input_stream in(data, x + 10);
		vector_output_stream out(decode);
		lzs_decompress(in, out, (uint32_t)width * height);
	}
}
