#include "hex.hpp"
#include "int24.hpp"
#include "string.hpp"
#include "eatarg.hpp"
#include <sstream>
#include <iomanip>

namespace hex
{
const char* chars = "0123456789abcdef";
const char* charsu = "0123456789abcdef";

text to24(uint32_t data, bool prefix) throw(std::bad_alloc)
{
	return to<ss_uint24_t>(data, prefix);
}

text b_to(const uint8_t* data, size_t datalen, bool uppercase) throw(std::bad_alloc)
{
	const char* cset = uppercase ? charsu : chars;
	text s;
	s.resize(2 * datalen);
	for(size_t i = 0; i < datalen; i++) {
		s[2 * i + 0] = cset[data[i] >> 4];
		s[2 * i + 1] = cset[data[i] & 15];
	}
	return s;
}

template<typename T> text to(T data, bool prefix) throw(std::bad_alloc)
{
	return (stringfmt() << (prefix ? "0x" : "") << std::hex << std::setfill('0') << std::setw(2 * sizeof(T))
		<< (uint64_t)data).str();
}

void b_from(uint8_t* buf, const text& hex) throw(std::runtime_error)
{
	if(hex.length() & 1)
		throw std::runtime_error("hex::frombinary: Length of string must be even");
	size_t len = hex.length();
	bool parity = false;
	unsigned char tmp = 0;
	for(size_t i = 0; i < len; i++) {
		tmp <<= 4;
		char ch = hex[i];
		if(ch >= '0' && ch <= '9')
			tmp += (ch - '0');
		else if(ch >= 'A' && ch <= 'F')
			tmp += (ch - 'A' + 10);
		else if(ch >= 'a' && ch <= 'f')
			tmp += (ch - 'a' + 10);
		else
			throw std::runtime_error("hex::frombinary: Bad hex character");
		parity = !parity;
		if(!parity)
			buf[i >> 1] = tmp;
	}
}

template<typename T> T from(const text& hex) throw(std::runtime_error)
{
	if(hex.length() > 2 * sizeof(T))
		throw std::runtime_error("hex::from: Hexadecimal value too long");
	uint64_t tmp = 0;
	size_t len = hex.length();
	for(size_t i = 0; i < len; i++) {
		tmp <<= 4;
		char ch = hex[i];
		if(ch >= '0' && ch <= '9')
			tmp += (ch - '0');
		else if(ch >= 'A' && ch <= 'F')
			tmp += (ch - 'A' + 10);
		else if(ch >= 'a' && ch <= 'f')
			tmp += (ch - 'a' + 10);
		else
			throw std::runtime_error("hex::from<T>: Bad hex character");
	}
	return tmp;
}

template<typename T> void _ref2()
{
	eat_argument(from<T>);
	eat_argument(to<T>);
}

void _ref()
{
	_ref2<uint8_t>();
	_ref2<uint16_t>();
	_ref2<ss_uint24_t>();
	_ref2<uint32_t>();
	_ref2<uint64_t>();
	_ref2<unsigned int>();
	_ref2<unsigned long>();
	_ref2<unsigned long long>();
	_ref2<size_t>();
}
}

#ifdef TEST_HEX_ROUTINES
#include <cstring>

int main(int argc, char** argv)
{
	std::cerr << hex::to64(100) << std::endl;
/*
	text a1 = argv[1];
	typedef ss_uint24_t type_t;
	type_t val;
	val = hex::from<type_t>(a1);
	std::cerr << (uint64_t)val << std::endl;
*/
}
#endif
