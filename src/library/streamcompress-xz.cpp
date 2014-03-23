#ifdef LIBLZMA_AVAILABLE
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
#include <lzma.h>
#include <stdexcept>

namespace
{
	struct lzma_options
	{
		bool xz;
		lzma_filter* fchain;
		lzma_check check;
		lzma_options_lzma lzmaopts;
	};

	struct lzma : public streamcompress::base
	{
		lzma(lzma_options& opts)
		{
			memset(&strm, 0, sizeof(strm));
			lzma_ret r;
			if(opts.xz) {
				r = lzma_stream_encoder(&strm, opts.fchain, opts.check);
			} else {
				r = lzma_alone_encoder(&strm, &opts.lzmaopts);
			}
			if(r >= 5) {
				if(r == LZMA_MEM_ERROR) throw std::runtime_error("Memory error");
				if(r == LZMA_MEMLIMIT_ERROR) throw std::runtime_error("Memory limit exceeded");
				if(r == LZMA_FORMAT_ERROR) throw std::runtime_error("File format error");
				if(r == LZMA_OPTIONS_ERROR) throw std::runtime_error("Options error");
				if(r == LZMA_DATA_ERROR) throw std::runtime_error("Data error");
				if(r == LZMA_BUF_ERROR) throw std::runtime_error("Buffer error");
				if(r == LZMA_PROG_ERROR) throw std::runtime_error("Programming error");
				throw std::runtime_error("Unknown error");
			}
		}
		~lzma()
		{
			lzma_end(&strm);
		}
		bool process(uint8_t*& in, size_t& insize, uint8_t*& out, size_t& outsize, bool final)
		{
			strm.next_in = in;
			strm.avail_in = insize;
			strm.next_out = out;
			strm.avail_out = outsize;
			lzma_ret r = lzma_code(&strm, final ? LZMA_FINISH : LZMA_RUN);
			in = (uint8_t*)strm.next_in;
			insize = strm.avail_in;
			out = strm.next_out;
			outsize = strm.avail_out;
			if(r == LZMA_STREAM_END) return true;
			if(r >= 5) {
				if(r == LZMA_MEM_ERROR) throw std::runtime_error("Memory error");
				if(r == LZMA_MEMLIMIT_ERROR) throw std::runtime_error("Memory limit exceeded");
				if(r == LZMA_FORMAT_ERROR) throw std::runtime_error("File format error");
				if(r == LZMA_OPTIONS_ERROR) throw std::runtime_error("Options error");
				if(r == LZMA_DATA_ERROR) throw std::runtime_error("Data error");
				if(r == LZMA_BUF_ERROR) throw std::runtime_error("Buffer error");
				if(r == LZMA_PROG_ERROR) throw std::runtime_error("Programming error");
				throw std::runtime_error("Unknown error");
			}
			return false;
		}
	private:
		lzma_stream strm;
	};

	struct foo {
		foo() {
			streamcompress::base::do_register("lzma", [](const std::string& v) -> streamcompress::base* {
				lzma_options opts;
				auto a = streamcompress::parse_attributes(v);
				unsigned level = 7;
				bool extreme = false;
				if(a.count("level")) level = parse_value<unsigned>(a["level"]);
				if(level > 9) level = 9;
				if(a.count("extreme")) extreme  = parse_value<bool>(a["level"]);
				opts.xz = false;
				lzma_lzma_preset(&opts.lzmaopts, level | (extreme ? LZMA_PRESET_EXTREME : 0));
				return new lzma(opts);
			});
			streamcompress::base::do_register("xz", [](const std::string& v) -> streamcompress::base* {
				lzma_options opts;
				auto a = streamcompress::parse_attributes(v);
				unsigned level = 7;
				bool extreme = false;
				lzma_options_lzma opt_lzma2;
				if(a.count("level")) level = parse_value<unsigned>(a["level"]);
				if(level > 9) level = 9;
				if(a.count("extreme")) extreme  = parse_value<bool>(a["level"]);
				lzma_lzma_preset(&opt_lzma2, level | (extreme ? LZMA_PRESET_EXTREME : 0));
				lzma_filter filterchain[] = {
					{ LZMA_FILTER_LZMA2, &opt_lzma2 },
					{ LZMA_VLI_UNKNOWN }
				};
				opts.fchain = filterchain;
				opts.check = LZMA_CHECK_CRC64;
				opts.xz = true;
				return new lzma(opts);
			});
		}
		~foo() {
			streamcompress::base::do_unregister("lzma");
			streamcompress::base::do_unregister("xz");
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

#ifdef STREAMCOMPRESS_LZMA_TEST
int main()
{
	std::vector<char> out;
	streamcompress::base* X = streamcompress::base::create_compressor("xz", "level=7,extreme=true");
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
#endif
