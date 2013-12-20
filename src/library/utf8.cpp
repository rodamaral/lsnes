#include <sstream>
#include "utf8.hpp"

namespace utf8
{
namespace
{
	//First nibble values:
	//0 => INITIAL
	//1 => S_2_2
	//2 => S_2_3
	//3 => S_2_4
	//4 => S_3_3
	//5 => S_3_4
	//6 => INIT_RE
	//7 => (unused)
	//8 => S_4_4
	//Second nibble values:
	//0 => Return NO CHARACTER and transition to another state with substate 0.
	//1 => Return the character and transition to another state with substate 0.
	//2 => Return invalid character and transition to another state with substate 0.
	//3 => Memorize character minus 192, return NO CHARACTER and transition to another state.
	//4 => Memorize character minus 224, return NO CHARACTER  and transition to another state.
	//5 => Memorize character minus 240, return NO CHARACTER  and transition to another state.
	//6 => Memorize byte, return invalid character and transition to another state.
	//7 => Return 2-byte value and transition to another state.
	//8 => Combine memorized, return NO CHARACTER and transition to another state.
	//9 => Return 3-byte value and transition to another state.
	//A => Return 4-byte value and transition to another state.
	//B => Handle memorized character and EOF.
	//C => Handle memorized character and continuation.
	const unsigned char transitions[] = {
		//E	//1	//C	//2	//3	//4	//I
		0x00,	0x01,	0x02,	0x13,	0x24,	0x35,	0x02,	//INITIAL
		0x01,	0x66,	0x07,	0x66,	0x66,	0x66,	0x66,	//S_2_2
		0x01,	0x66,	0x48,	0x66,	0x66,	0x66,	0x66,	//S_2_3
		0x01,	0x66,	0x58,	0x66,	0x66,	0x66,	0x66,	//S_2_4
		0x01,	0x66,	0x09,	0x66,	0x66,	0x66,	0x66,	//S_3_3
		0x01,	0x66,	0x88,	0x66,	0x66,	0x66,	0x66,	//S_3_4
		0x0B,	0x6C,	0x6C,	0x6C,	0x6C,	0x6C,	0x6C,	//INIT_RE
		0x01,	0x66,	0x0A,	0x66,	0x66,	0x66,	0x66	//S_4_4
	};
}

extern const uint16_t initial_state = 0;

int32_t parse_byte(int ch, uint16_t& state) throw()
{
	unsigned char mch = (ch < 248) ? ch : 248;
	uint32_t astate = state >> 12;
	uint32_t iclass;
	uint32_t tmp;
	if(astate > 7)		astate = 7;
	if(ch < 0)		iclass = 0;
	else if(ch < 128)	iclass = 1;
	else if(ch < 192)	iclass = 2;
	else if(ch < 224)	iclass = 3;
	else if(ch < 240)	iclass = 4;
	else if(ch < 248)	iclass = 5;
	else			iclass = 6;
	unsigned char ctrl = transitions[astate * 7 + iclass];

	switch(ctrl & 0xF) {
	case 0x0:
		state = (ctrl & 0xF0) * 256;
		return -1;
	case 0x1:
		state = (ctrl & 0xF0) * 256;
		return ch;
	case 0x2:
		state = (ctrl & 0xF0) * 256;
		return 0xFFFD;
	case 0x3:
		state = (ctrl & 0xF0) * 256 + ch - 192;
		return -1;
	case 0x4:
		state = (ctrl & 0xF0) * 256 + ch - 224;
		return -1;
	case 0x5:
		state = (ctrl & 0xF0) * 256 + ch - 240;
		return -1;
	case 0x6:
		state = (ctrl & 0xF0) * 256 + mch;
		return 0xFFFD;
	case 0x7:
		tmp = (state & 0xFFF) * 64 + ch - 128;
		if(tmp < 0x80)
			tmp = 0xFFFD;
		state = (ctrl & 0xF0) * 256;
		return tmp;
	case 0x8:
		state = (ctrl & 0xF0) * 256 + (state & 0xFFF) * 64 + ch - 128;
		return -1;
	case 0x9:
		tmp = (state & 0xFFF) * 64 + ch - 128;
		if(tmp < 0x800 || (tmp & 0xF800) == 0xD800 || (tmp & 0xFFFE) == 0xFFFE)
			tmp = 0xFFFD;
		state = (ctrl & 0xF0) * 256;
		return tmp;
	case 0xA:
		tmp = (state & 0x7FFF) * 64 + ch - 128;
		if(tmp < 0x10000 || tmp > 0x10FFFD || (tmp & 0xFFFE) == 0xFFFE)
			tmp = 0xFFFD;
		state = (ctrl & 0xF0) * 256;
		return tmp;
	case 0xB:
		if(state & 0x80)
			tmp = 0xFFFD;
		else
			tmp = state & 0x7F;
		state = (ctrl & 0xF0) * 256;
		return tmp;
	case 0xC:
		//This is nasty.
		if((state & 0x80) == 0) {
			tmp = state & 0x7F;
			state = 0x6000 + mch;
			return tmp;
		} else if((state & 0xF8) == 0xF8 || (state & 0xF8) == 0x80) {
			//Continuation or invalid.
			state = 0x6000 + mch;
			return 0xFFFD;
		} else if(iclass == 0) {
			//Incomplete.
			state = 0;
			return 0xFFFD;
		} else if(iclass != 2) {
			//Bad sequence.
			state = 0x6000 + mch;
			return 0xFFFD;
		} else if((state & 0xE0) == 0xC0) {
			//Complete 2-byte sequence.
			tmp = (state & 0x1F) * 64 + (ch & 0x3F);
			state = 0;
			if(tmp < 0x80)
				tmp = 0xFFFD;
			return tmp;
		} else if((state & 0xF0) == 0xE0) {
			//First 2 bytes of 3-byte sequence.
			state = 0x4000 + (state & 0x0F) * 64 + (ch & 0x3F);
			return -1;
		} else if((state & 0xF8) == 0xF0) {
			//First 2 bytes of 4-byte sequence.
			state = 0x5000 + (state & 0x07) * 64 + (ch & 0x3F);
			return -1;
		}
	};
	return -1;
}

size_t strlen(const std::string& str) throw()
{
	uint16_t s = initial_state;
	size_t r = 0;
	for(size_t i = 0; i < str.length(); i++)
		if(parse_byte(static_cast<uint8_t>(str[i]), s) >= 0)
			r++;
	if(parse_byte(-1, s) >= 0)
		r++;
	return r;
}

std::u32string to32(const std::string& utf8)
{
	std::u32string x;
	x.resize(strlen(utf8));
	to32i(utf8.begin(), utf8.end(), x.begin());
	return x;
}

std::string to8(const std::u32string& utf32)
{
	std::ostringstream s;
	for(auto i : utf32) {
		if(i < 0x80)
			s << (unsigned char)i;
		else if(i < 0x800)
			s << (unsigned char)(0xC0 + (i >> 6)) << (unsigned char)(0x80 + (i & 0x3F));
		else if(i < 0x10000)
			s << (unsigned char)(0xE0 + (i >> 12)) << (unsigned char)(0x80 + ((i >> 6) & 0x3F))
				 << (unsigned char)(0x80 + (i & 0x3F));
		else if(i < 0x10FFFF)
			s << (unsigned char)(0xF0 + (i >> 18)) << (unsigned char)(0x80 + ((i >> 12) & 0x3F))
				<< (unsigned char)(0x80 + ((i >> 6) & 0x3F))
				<< (unsigned char)(0x80 + (i & 0x3F));
	}
	return s.str();
}
}

#ifdef TEST_UTF8
#include <iostream>
char* format_dword(uint16_t s)
{
	static char buf[32];
	sprintf(buf, "%04X", s);
	return buf;
}

int main()
{
	uint16_t s = utf8::initial_state;
	while(true) {
		int c;
		int32_t d;
		std::cin >> c;
		d = utf8::parse_byte(c, s);
		std::cout << "> " << d << " (status word=" << format_dword(s) << ")" << std::endl;
		if(c == -1 && d == -1)
			return 0;
	}
	return 0;
}
#endif
