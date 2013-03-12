#ifndef _library__utf8__hpp__included__
#define _library__utf8__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <string>

/**
 * Initial state for UTF-8 parser.
 */
extern const uint16_t utf8_initial_state;
/**
 * Parse a byte.
 *
 * Parameter ch: The character to parse. -1 for end of string.
 * Parameter state: The state. Mutated.
 * Returns: The codepoint, or -1 if no codepoint emitted.
 *
 * Note: When called with EOF, max 1 codepoint can be emitted.
 */
int32_t utf8_parse_byte(int ch, uint16_t& state) throw();
/**
 * Return length of string in UTF-8 codepoints.
 *
 * Parameter str: The string.
 * Returns: The length in codepoints.
 */
size_t utf8_strlen(const std::string& str) throw();

/**
 * Iterator copy from UTF-8 to UTF-32
 */
template<typename srcitr, typename dstitr>
inline void copy_from_utf8(srcitr begin, srcitr end, dstitr target)
{
	uint16_t state = utf8_initial_state;
	for(srcitr i = begin; i != end; i++) {
		int32_t x = utf8_parse_byte(*i, state);
		if(x >= 0) {
			*target = x;
			++target;
		}
	}
	int32_t x = utf8_parse_byte(-1, state);
	if(x >= 0) {
		*target = x;
		++target;
	}
}

#endif
