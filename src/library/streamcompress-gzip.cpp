#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include "streamcompress.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include <zlib.h>
#include <stdexcept>

namespace
{
	void* zalloc(void* opaque, unsigned items, unsigned size)
	{
		return calloc(items, size);
	}

	void zfree(void* opaque, void* ptr)
	{
		free(ptr);
	}

	struct gzip : public streamcompress::base
	{
		gzip(unsigned level)
		{
			memset(&strm, 0, sizeof(z_stream));
			strm.zalloc = zalloc;
			strm.zfree = zfree;
			if(level > 9) level = 9;
			deflateInit2(&strm, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
			crc = crc32(0, NULL, 0);
			size = 0;
			hdr = 0;
			trl = 0;
			data_output = false;
		}
		~gzip()
		{
			deflateEnd(&strm);
		}
		bool process(uint8_t*& in, size_t& insize, uint8_t*& out, size_t& outsize, bool final)
		{
			uint8_t header[] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 255};
			uint8_t trailer[8];
			serialization::u32l(trailer + 0, crc);
			serialization::u32l(trailer + 4, size);
			while(hdr < 10) {
				if(!outsize) return false;
				*(out++) = header[hdr++];
				outsize--;
			}
			if(data_output) {
				while(trl < 8) {
					if(!outsize) return false;
					*(out++) = trailer[trl++];
					outsize--;
				}
				return true;
			}
			strm.next_in = in;
			strm.avail_in = insize;
			strm.next_out = out;
			strm.avail_out = outsize;
			int r = deflate(&strm, final ? Z_FINISH : 0);
			size += (insize - strm.avail_in);
			crc = crc32(crc, in, insize - strm.avail_in);
			in = strm.next_in;
			insize = strm.avail_in;
			out = strm.next_out;
			outsize = strm.avail_out;
			if(r == Z_STREAM_END) data_output = true;
			if(r < 0) {
				if(r == Z_ERRNO) throw std::runtime_error("OS error");
				if(r == Z_STREAM_ERROR) throw std::runtime_error("Streams error");
				if(r == Z_DATA_ERROR) throw std::runtime_error("Data error");
				if(r == Z_MEM_ERROR) throw std::runtime_error("Memory error");
				if(r == Z_BUF_ERROR) throw std::runtime_error("Buffer error");
				if(r == Z_VERSION_ERROR) throw std::runtime_error("Version error");
				throw std::runtime_error("Unknown error");
			}
			return false;
		}
	private:
		z_stream strm;
		uint32_t crc;
		uint32_t size;
		unsigned hdr;
		unsigned trl;
		bool data_output;
	};

	struct foo {
		foo() {
			streamcompress::base::do_register("gzip", [](const std::string& v) ->
				streamcompress::base* {
				auto a = streamcompress::parse_attributes(v);
				unsigned compression = 7;
				if(a.count("level")) compression = parse_value<unsigned>(a["level"]);
				return new gzip(compression);
			});
		}
		~foo() {
			streamcompress::base::do_unregister("gzip");
		}
	} _foo;

	class stdin_input
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::source_tag category;

		stdin_input()
		{
		}

		void close()
		{
		}

		std::streamsize read(char* s, std::streamsize x)
		{
			std::cin.read(s, x);
			if(!std::cin.gcount() && !std::cin) return -1;
			return std::cin.gcount();
		}

		~stdin_input()
		{
		}
	private:
		stdin_input& operator=(const stdin_input& f);
	};

};

#ifdef STREAMCOMPRESS_GZIP_TEST
int main()
{
	std::vector<char> out;
	streamcompress::base* X = streamcompress::base::create_compressor("gzip", "level=7");
	boost::iostreams::filtering_istream* s = new boost::iostreams::filtering_istream();
	s->push(streamcompress::iostream(X));
	s->push(stdin_input());
	boost::iostreams::back_insert_device<std::vector<char>> rd(out);
	boost::iostreams::copy(*s, rd);
	delete s;
	delete X;

	std::cout.write(&out[0], out.size());
	return 0;
}
#endif
