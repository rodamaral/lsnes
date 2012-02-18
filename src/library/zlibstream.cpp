#include "library/zlibstream.hpp"
#include "library/minmax.hpp"
#include <stdexcept>
#include <cstring>
#include <cerrno>

namespace
{
	void throw_zlib_error(int x)
	{
		switch(x) {
		case Z_NEED_DICT:	throw std::runtime_error("Dictionary needed");
		case Z_ERRNO:		throw std::runtime_error(std::string("OS error: ") + strerror(errno));
		case Z_STREAM_ERROR:	throw std::runtime_error("Stream error");
		case Z_DATA_ERROR:	throw std::runtime_error("Data error");
		case Z_MEM_ERROR:	throw std::bad_alloc();
		case Z_BUF_ERROR:	throw std::runtime_error("Buffer error");
		case Z_VERSION_ERROR:	throw std::runtime_error("Version error");
		case Z_OK:
		case Z_STREAM_END:
			break;
		default:		throw std::runtime_error("Unknown error");
		};
	}
}

zlibstream::zlibstream(unsigned compression)
{
	streamsize = 0;
	memset(&z, 0, sizeof(z));
	throw_zlib_error(deflateInit(&z, compression));
}

zlibstream::~zlibstream()
{
	deflateEnd(&z);
}

void zlibstream::reset(uint8_t* data, size_t datalen)
{
	std::list<struct page> tmp;
	size_t odatalen = datalen;
	while(datalen > 0) {
		tmp.push_back(page());
		size_t copylen = min(datalen, static_cast<size_t>(ZLIB_PAGE_STORAGE));
		memcpy(&tmp.back().data[0], data, copylen);
		tmp.back().used = copylen;
		data += copylen;
		datalen -= copylen;
	}
	deflateReset(&z);
	streamsize = odatalen;
	flag = false;
	tmp.swap(storage);
}

void zlibstream::write(uint8_t* data, size_t datalen)
{
	flushpage(data, datalen, false);
}

void zlibstream::read(std::vector<char>& out)
{
	flushpage(NULL, 0, true);
	out.resize(streamsize);
	size_t itr = 0;
	for(auto& i : storage) {
		memcpy(&out[itr], i.data, i.used);
		itr += i.used;
	}
}

extern void* orig_return_address;
extern void* orig_return_address2;

void zlibstream::flushpage(uint8_t* data, size_t datalen, bool final)
{
	z.next_in = data;
	z.avail_in = datalen;
	while(z.avail_in || final) {
		if(storage.empty() || storage.back().used == ZLIB_PAGE_STORAGE) {
			storage.push_back(page());
			storage.back().used = 0;
		}
		z.next_out = storage.back().data + storage.back().used;
		z.avail_out = ZLIB_PAGE_STORAGE - storage.back().used;
		size_t itmp = z.avail_out;
		int x = deflate(&z, final ? Z_FINISH : 0);
		storage.back().used += (itmp - z.avail_out);
		streamsize += (itmp - z.avail_out);
		throw_zlib_error(x);
		if(final && x == Z_STREAM_END)
			break;
	}
}

void zlibstream::set_flag(bool f)
{
	flag = f;
}

bool zlibstream::get_flag()
{
	return flag;
}
