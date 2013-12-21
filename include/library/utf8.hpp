#ifndef _library__utf8__hpp__included__
#define _library__utf8__hpp__included__

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <functional>

namespace utf8
{
/**
 * Initial state for UTF-8 parser.
 */
extern const uint16_t initial_state;
/**
 * Parse a byte.
 *
 * Parameter ch: The character to parse. -1 for end of string.
 * Parameter state: The state. Mutated.
 * Returns: The codepoint, or -1 if no codepoint emitted.
 *
 * Note: When called with EOF, max 1 codepoint can be emitted.
 */
int32_t parse_byte(int ch, uint16_t& state) throw();
/**
 * Return length of string in UTF-8 codepoints.
 *
 * Parameter str: The string.
 * Returns: The length in codepoints.
 */
size_t strlen(const std::string& str) throw();

/**
 * Transform UTF-8 into UTF-32.
 */
std::u32string to32(const std::string& utf8);

/**
 * Transform UTF-32 into UTF-8.
 */
std::string to8(const std::u32string& utf32);

/**
 * Iterator copy from UTF-8 to UTF-32
 */
template<typename srcitr, typename dstitr>
inline void to32i(srcitr begin, srcitr end, dstitr target)
{
	uint16_t state = initial_state;
	for(srcitr i = begin; i != end; i++) {
		int32_t x = parse_byte((unsigned char)*i, state);
		if(x >= 0) {
			*target = x;
			++target;
		}
	}
	int32_t x = parse_byte(-1, state);
	if(x >= 0) {
		*target = x;
		++target;
	}
}

}
#endif
