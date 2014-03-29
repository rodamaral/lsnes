#ifndef _library__streamcompress__hpp__included__
#define _library__streamcompress__hpp__included__

#include <iosfwd>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/operations.hpp>
#include <string>
#include <set>
#include <map>
#include <functional>
#include "minmax.hpp"
#include <cstring>

namespace streamcompress
{
std::map<std::string, std::string> parse_attributes(const std::string& val);

class base
{
public:
	virtual ~base();
/**
 * Compress data.
 *
 * Parameter in: Input stream pointer. Updated.
 * Parameter insize: Input stream available. Updated.
 * Parameter out: Output stream pointer. Updated.
 * Parameter outsize: Output stream available. Updated.
 * Parameter final: True if input stream ends after currently available data, false otherwise.
 * Returns: True if EOS has been seen and all data is emitted, otherwise false.
 */
	virtual bool process(uint8_t*& in, size_t& insize, uint8_t*& out, size_t& outsize, bool final) = 0;

	static std::set<std::string> get_compressors();
	static base* create_compressor(const std::string& name, const std::string& args);
	static void do_register(const std::string& name,
		std::function<base*(const std::string&)> ctor);
	static void do_unregister(const std::string& name);
};

class iostream
{
public:
	typedef char char_type;
	struct category : boost::iostreams::input_filter_tag, boost::iostreams::multichar_tag {};
/**
 * Createa a new compressing stream.
 */
	iostream(base* _compressor)
	{
		compressor = _compressor;
		inbuf_use = 0;
		outbuf_use = 0;
		ieof_flag = false;
		oeof_flag = false;
		emitted = 0;
	}
	//Other methods.
	template<typename Source>
	std::streamsize read(Source& src, char* s, std::streamsize n)
	{
		size_t flushed = 0;
		while(n > 0) {
			if(oeof_flag && outbuf_use == 0) {
				if(flushed) return flushed;
				return -1;
			}
			if(outbuf_use > 0) {
				size_t tocopy = min(outbuf_use, (size_t)n);
				size_t left = outbuf_use - tocopy;
				memcpy(s, outbuffer, tocopy);
				s += tocopy;
				n -= tocopy;
				flushed += tocopy;
				outbuf_use -= tocopy;
				memmove(outbuffer, outbuffer + tocopy, left);
				emitted += tocopy;
			}
			while(!ieof_flag && inbuf_use < sizeof(inbuffer)) {
				std::streamsize r = boost::iostreams::read(src, (char*)inbuffer + inbuf_use,
					sizeof(inbuffer) - inbuf_use);
				if(r == -1) {
					ieof_flag = true;
					break;
				}
				inbuf_use += r;
			}
			if(!oeof_flag) {
				uint8_t* in = inbuffer;
				size_t insize = inbuf_use;
				uint8_t* out = outbuffer + outbuf_use;
				size_t outsize = sizeof(outbuffer) - outbuf_use;
				oeof_flag = compressor->process(in, insize, out, outsize, ieof_flag);
				outbuf_use = sizeof(outbuffer) - outsize;
				size_t in_r = inbuf_use - insize;
				memmove(inbuffer, inbuffer + in_r, insize);
				inbuf_use = insize;
			}
		}
		return flushed;
	}
	template<typename Source>
	void close(Source& src)
	{
	}
private:
	base* compressor;
	uint8_t inbuffer[4096];
	uint8_t outbuffer[4096];
	size_t inbuf_use;
	size_t outbuf_use;
	bool ieof_flag;
	bool oeof_flag;
	size_t emitted;
};
}

#endif
