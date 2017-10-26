#include "binarystream.hpp"
#include "serialization.hpp"
#include "minmax.hpp"
#include "string.hpp"
#include <functional>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <unistd.h>

//Damn Windows.
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

namespace
{
	void write_whole(int s, const char* buf, size_t size)
	{
		size_t w = 0;
		while(w < size) {
			int maxw = 32767;
			if((size_t)maxw > (size - w))
				maxw = size - w;
			int r = write(s, buf + w, maxw);
			if(r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if(r < 0) {
				int err = errno;
				(stringfmt() << strerror(err)).throwex();
			}
			w += r;
		}
	}

	size_t whole_read(int s, char* buf, size_t size)
	{
		size_t r = 0;
		while(r < size) {
			int maxr = 32767;
			if((size_t)maxr > (size - r))
				maxr = size - r;
			int x = read(s, buf + r, maxr);
			if(x < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if(x < 0) {
				int err = errno;
				(stringfmt() << strerror(err)).throwex();
			}
			if(x == 0)
				break;	//EOF.
			r += x;
		}
		return r;
	}
}

namespace binarystream
{
const uint32_t TAG_ = 0xaddb2d86;

output::output()
	: strm(-1)
{
}

output::output(int s)
	: strm(s)
{
}

void output::byte(uint8_t byte)
{
	write(reinterpret_cast<char*>(&byte), 1);
}

void output::number(uint64_t number)
{
	char data[10];
	size_t len = 0;
	do {
		bool cont = (number > 127);
		data[len++] = (cont ? 0x80 : 0x00) | (number & 0x7F);
		number >>= 7;
	} while(number);
	write(data, len);
}

size_t output::numberbytes(uint64_t number)
{
	size_t o = 0;
	do {
		o++;
		number >>= 7;
	} while(number);
	return o;
}

size_t output::stringbytes(const std::string& string)
{
	size_t slen = string.length();
	return numberbytes(slen) + slen;
}

void output::number32(uint32_t number)
{
	char data[4];
	serialization::u32b(data, number);
	write(data, 4);
}

void output::string(const std::string& string)
{
	number(string.length());
	std::vector<char> tmp(string.begin(), string.end());
	write(&tmp[0], tmp.size());
}

void output::string_implicit(const std::string& string)
{
	std::vector<char> tmp(string.begin(), string.end());
	write(&tmp[0], tmp.size());
}

void output::blob_implicit(const std::vector<char>& blob)
{
	write(&blob[0], blob.size());
}

void output::raw(const void* buf, size_t bufsize)
{
	write(reinterpret_cast<const char*>(buf), bufsize);
}

void output::write_extension_tag(uint32_t tag, uint64_t size)
{
	number32(TAG_);
	number32(tag);
	number(size);
}

void output::extension(uint32_t tag, std::function<void(output&)> fn, bool even_empty)
{
	output tmp;
	fn(tmp);
	if(!even_empty && !tmp.buf.size())
		return;
	number32(TAG_);
	number32(tag);
	number(tmp.buf.size());
	blob_implicit(tmp.buf);
}

void output::extension(uint32_t tag, std::function<void(output&)> fn, bool even_empty,
	size_t size_precognition)
{
	if(!even_empty && !size_precognition)
		return;
	number32(TAG_);
	number32(tag);
	number(size_precognition);
	fn(*this);
}

void output::write(const char* ibuf, size_t size)
{
	if(strm >= 0)
		write_whole(strm, ibuf, size);
	else {
		size_t o = buf.size();
		buf.resize(o + size);
		memcpy(&buf[o], ibuf, size);
	}
}

std::string output::get()
{
	if(strm >= 0)
		throw std::logic_error("Get can only be used without explicit sink");
	return std::string(buf.begin(), buf.end());
}

uint8_t input::byte()
{
	char byte;
	read(&byte, 1);
	return byte;
}

uint64_t input::number()
{
	uint64_t s = 0;
	int sh = 0;
	uint8_t c;
	do {
		read(reinterpret_cast<char*>(&c), 1);
		s |= (static_cast<uint64_t>(c & 0x7F) << sh);
		sh += 7;
	} while(c & 0x80);
	return s;
}

uint32_t input::number32()
{
	char c[4];
	read(c, 4);
	return serialization::u32b(c);
}

std::string input::string()
{
	size_t sz = number();
	std::vector<char> _r;
	_r.resize(sz);
	read(&_r[0], _r.size());
	std::string r(_r.begin(), _r.end());
	return r;
}

std::string input::string_implicit()
{
	if(!parent)
		throw std::logic_error("binarystream::input::string_implicit() can only be used in substreams");
	std::vector<char> _r;
	_r.resize(left);
	read(&_r[0], left);
	std::string r(_r.begin(), _r.end());
	return r;
}

void input::blob_implicit(std::vector<char>& blob)
{
	if(!parent)
		throw std::logic_error("binarystream::input::string_implicit() can only be used in substreams");
	blob.resize(left);
	read(&blob[0], left);
}

input::input(int s)
	: parent(NULL), strm(s), left(0)
{
}

input::input(input& s, uint64_t len)
	: parent(&s), strm(s.strm), left(len)
{
	if(parent->parent && left > parent->left)
		throw std::runtime_error("Substream length greater than its parent");
}

void input::raw(void* buf, size_t bufsize)
{
	read(reinterpret_cast<char*>(buf), bufsize);
}

void input::extension(std::function<void(uint32_t tag, input& s)> fn)
{
	extension({}, fn);
}

void input::extension(std::initializer_list<binary_tag_handler> funcs,
	std::function<void(uint32_t tag, input& s)> default_hdlr)
{
	std::map<uint32_t, std::function<void(input& s)>> fn;
	for(auto i : funcs)
		fn[i.tag] = i.fn;
	while(!parent || left > 0) {
		char c[4];
		if(!read(c, 4, true))
			break;
		uint32_t tagid = serialization::u32b(c);
		if(tagid != TAG_)
			throw std::runtime_error("Binary file packet structure desync");
		uint32_t tag = number32();
		uint64_t size = number();
		input ss(*this, size);
		if(fn.count(tag))
			fn[tag](ss);
		else
			default_hdlr(tag, ss);
		ss.flush();
	}
}

void input::flush()
{
	if(!parent)
		throw std::logic_error("binarystream::input::flush() can only be used in substreams");
	char buf[256];
	while(left)
		read(buf, min(left, (uint64_t)256));
}

bool input::read(char* buf, size_t size, bool allow_none)
{
	if(parent) {
		if(left == 0 && allow_none)
			return false;
		if(size > left)
			std::runtime_error("Substream unexpected EOF");
		parent->read(buf, size, false);
		left -= size;
	} else {
		size_t r = whole_read(strm, buf, size);
		if(r < size) {
			if(!r && allow_none)
				return false;
			throw std::runtime_error("Unexpected EOF");
		}
	}
	return true;
}

void null_default(uint32_t tag, input& s)
{
	//no-op
}
}
