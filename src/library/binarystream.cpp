#include "binarystream.hpp"
#include "serialization.hpp"
#include "minmax.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>

namespace binarystream
{
const uint32_t TAG_ = 0xaddb2d86;

output::output()
	: strm(buf)
{
}

output::output(std::ostream& s)
	: strm(s)
{
}

void output::byte(uint8_t byte)
{
	strm.write(reinterpret_cast<char*>(&byte), 1);
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
	strm.write(data, len);
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

void output::number32(uint32_t number)
{
	char data[4];
	serialization::u32b(data, number);
	strm.write(data, 4);
}

void output::string(const std::string& string)
{
	number(string.length());
	std::copy(string.begin(), string.end(), std::ostream_iterator<char>(strm));
}

void output::string_implicit(const std::string& string)
{
	std::copy(string.begin(), string.end(), std::ostream_iterator<char>(strm));
}

void output::blob_implicit(const std::vector<char>& blob)
{
	strm.write(&blob[0], blob.size());
}

void output::raw(const void* buf, size_t bufsize)
{
	strm.write(reinterpret_cast<const char*>(buf), bufsize);
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
	std::string str = tmp.get();
	if(!even_empty && !str.length())
		return;
	number32(TAG_);
	number32(tag);
	string(str);
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


std::string output::get()
{
	if(&strm != &buf)
		throw std::logic_error("Get can only be used without explicit sink");
	return buf.str();
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

input::input(std::istream& s)
	: strm(s), left(0), parent(NULL)
{
}

input::input(input& s, uint64_t len)
	: strm(s.strm), left(len), parent(&s)
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
		strm.read(buf, size);
		if(!strm) {
			if(!strm.gcount() && allow_none)
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
