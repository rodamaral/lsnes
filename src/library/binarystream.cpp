#include "binarystream.hpp"
#include "serialization.hpp"
#include "minmax.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>

const uint32_t TAG_ = 0xaddb2d86;

binary_output_stream::binary_output_stream()
	: strm(buf)
{
}

binary_output_stream::binary_output_stream(std::ostream& s)
	: strm(s)
{
}

void binary_output_stream::byte(uint8_t byte)
{
	strm.write(reinterpret_cast<char*>(&byte), 1);
}

void binary_output_stream::number(uint64_t number)
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

void binary_output_stream::number32(uint32_t number)
{
	char data[4];
	write32ube(data, number);
	strm.write(data, 4);
}

void binary_output_stream::string(const std::string& string)
{
	number(string.length());
	std::copy(string.begin(), string.end(), std::ostream_iterator<char>(strm));
}

void binary_output_stream::string_implicit(const std::string& string)
{
	std::copy(string.begin(), string.end(), std::ostream_iterator<char>(strm));
}

void binary_output_stream::blob_implicit(const std::vector<char>& blob)
{
	strm.write(&blob[0], blob.size());
}

void binary_output_stream::raw(const void* buf, size_t bufsize)
{
	strm.write(reinterpret_cast<const char*>(buf), bufsize);
}

void binary_output_stream::write_extension_tag(uint32_t tag, uint64_t size)
{
	number32(TAG_);
	number32(tag);
	number(size);
}

void binary_output_stream::extension(uint32_t tag, std::function<void(binary_output_stream&)> fn, bool even_empty)
{
	binary_output_stream tmp;
	fn(tmp);
	std::string str = tmp.get();
	if(!even_empty && !str.length())
		return;
	number32(TAG_);
	number32(tag);
	string(str);
}

std::string binary_output_stream::get()
{
	if(&strm != &buf)
		throw std::logic_error("Get can only be used without explicit sink");
	return buf.str();
}

uint8_t binary_input_stream::byte()
{
	char byte;
	read(&byte, 1);
	return byte;
}

uint64_t binary_input_stream::number()
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

uint32_t binary_input_stream::number32()
{
	char c[4];
	read(c, 4);
	return read32ube(c);
}

std::string binary_input_stream::string()
{
	size_t sz = number();
	std::vector<char> _r;
	_r.resize(sz);
	read(&_r[0], _r.size());
	std::string r(_r.begin(), _r.end());
	return r;
}

std::string binary_input_stream::string_implicit()
{
	if(implicit_len)
		throw std::logic_error("binary_input_stream::string_implicit() can only be used in substreams");
	std::vector<char> _r;
	_r.resize(left);
	read(&_r[0], left);
	std::string r(_r.begin(), _r.end());
	return r;
}

void binary_input_stream::blob_implicit(std::vector<char>& blob)
{
	if(implicit_len)
		throw std::logic_error("binary_input_stream::string_implicit() can only be used in substreams");
	blob.resize(left);
	read(&blob[0], left);
}

binary_input_stream::binary_input_stream(std::istream& s)
	: strm(s), implicit_len(true), left(0)
{
}

binary_input_stream::binary_input_stream(std::istream& s, uint64_t len)
	: strm(s), implicit_len(false), left(len)
{
}

void binary_input_stream::raw(void* buf, size_t bufsize)
{
	read(reinterpret_cast<char*>(buf), bufsize);
}

void binary_input_stream::extension(std::function<void(uint32_t tag, binary_input_stream& s)> fn)
{
	extension({}, fn);
}

void binary_input_stream::extension(std::initializer_list<binary_tag_handler> funcs,
	std::function<void(uint32_t tag, binary_input_stream& s)> default_hdlr)
{
	std::map<uint32_t, std::function<void(binary_input_stream& s)>> fn;
	for(auto i : funcs)
		fn[i.tag] = i.fn;
	while(implicit_len || left > 0) {
		char c[4];
		if(!read(c, 4, true))
			break;
		uint32_t tagid = read32ube(c);
		if(tagid != TAG_)
			throw std::runtime_error("Binary file packet structure desync");
		uint32_t tag = number32();
		uint64_t size = number();
		binary_input_stream ss(strm, size);
		if(fn.count(tag))
			fn[tag](ss);
		else
			default_hdlr(tag, ss);
		ss.flush();
	}
}

void binary_input_stream::flush()
{
	if(implicit_len)
		throw std::logic_error("binary_input_stream::flush() can only be used in substreams");
	char buf[256];
	while(left)
		read(buf, min(left, (uint64_t)256));
}

bool binary_input_stream::read(char* buf, size_t size, bool allow_none)
{
	if(!implicit_len && size > left)
		throw std::runtime_error("Substream unexpected EOF");
	strm.read(buf, size);
	if(!strm) {
		if(!strm.gcount() && allow_none)
			return false;
		throw std::runtime_error("Unexpected EOF");
	}
	left -= size;
	return true;
}

void binary_null_default(uint32_t tag, binary_input_stream& s)
{
	//no-op
}
