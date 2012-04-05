#ifndef _library__bintohex__hpp__included__
#define _library__bintohex__hpp__included__

#include <cstdint>
#include <string>
#include <cstdlib>

/**
 * Transform binary data into hex string.
 *
 * Parameter data: The data to transform.
 * Parameter datalen: Number of bytes of data.
 * Returns: Hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string binary_to_hex(const uint8_t* data, size_t datalen) throw(std::bad_alloc);


#endif
