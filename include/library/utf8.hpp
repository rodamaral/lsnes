#ifndef _utf8__hpp__included__
#define _utf8__hpp__included__

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

#endif
