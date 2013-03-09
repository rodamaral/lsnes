#include "png.hpp"

#include <fstream>
#include <iostream>
#include <cstdint>
#include <zlib.h>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

namespace
{
	void encode32(char* _buf, uint32_t val) throw()
	{
		unsigned char* buf = reinterpret_cast<unsigned char*>(_buf);
		buf[0] = ((val >> 24) & 0xFF);
		buf[1] = ((val >> 16) & 0xFF);
		buf[2] = ((val >> 8) & 0xFF);
		buf[3] = (val & 0xFF);
	}

	class png_hunk_output
	{
	public:
		typedef char char_type;
		struct category : boost::iostreams::closable_tag, boost::iostreams::sink_tag {};
		png_hunk_output(std::ostream& _os, uint32_t _type)
			: os(_os), type(_type)
		{
		}

		void close()
		{
			uint32_t crc = crc32(0, NULL, 0);
			char fixed[12];
			encode32(fixed, stream.size());
			encode32(fixed + 4, type);
			crc = crc32(crc, reinterpret_cast<Bytef*>(fixed + 4), 4);
			if(stream.size() > 0)
				crc = crc32(crc, reinterpret_cast<Bytef*>(&stream[0]), stream.size());
			encode32(fixed + 8, crc);
			os.write(fixed, 8);
			os.write(&stream[0], stream.size());
			os.write(fixed + 8, 4);
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t oldsize = stream.size();
			stream.resize(oldsize + n);
			memcpy(&stream[oldsize], s, n);
			return n;
		}
	protected:
		std::vector<char> stream;
		std::ostream& os;
		uint32_t type;
	};
}

void save_png_data(const std::string& file, uint8_t* data24, uint32_t width, uint32_t height) throw(std::bad_alloc,
	std::runtime_error)
{
	char* data = reinterpret_cast<char*>(data24);
	std::ofstream filp(file.c_str(), std::ios_base::binary);
	if(!filp)
		throw std::runtime_error("Can't open target PNG file");
	char png_magic[] = {-119, 80, 78, 71, 13, 10, 26, 10};
	filp.write(png_magic, sizeof(png_magic));
	char ihdr[] = {25, 25, 25, 25, 25, 25, 25, 25, 8, 2, 0, 0, 0};
	boost::iostreams::stream<png_hunk_output> ihdr_h(filp, 0x49484452);
	encode32(ihdr, width);
	encode32(ihdr + 4, height);
	ihdr_h.write(ihdr, sizeof(ihdr));
	ihdr_h.close();

	boost::iostreams::filtering_ostream idat_h;
	boost::iostreams::zlib_params params;
	params.noheader = false;
	idat_h.push(boost::iostreams::zlib_compressor(params));
	idat_h.push(png_hunk_output(filp, 0x49444154));
	for(uint32_t i = 0; i < height; i++) {
		char identity_filter = 0;
		idat_h.write(&identity_filter, 1);
		idat_h.write(data + i * 3 * width, 3 * width);
	}
	idat_h.pop();
	idat_h.pop();

	boost::iostreams::stream<png_hunk_output> iend_h(filp, 0x49454E44);
	iend_h.close();
	if(!filp)
		throw std::runtime_error("Can't write target PNG file");
}
