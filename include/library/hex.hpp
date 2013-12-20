#ifndef _library__hex__hpp__included__
#define _library__hex__hpp__included__

#include <cstdint>
#include <string>
#include <cstdlib>
#include <stdexcept>

namespace hex
{
/**
 * Transform binary data into hex string.
 *
 * Parameter data: The data to transform.
 * Parameter datalen: Number of bytes of data.
 * Parameter uppercase: Use uppercase?
 * Returns: Hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string b_to(const uint8_t* data, size_t datalen, bool uppercase = false) throw(std::bad_alloc);

/**
 * Transform unsigned integer into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
template<typename T> std::string to(T data, bool prefix = false) throw(std::bad_alloc);

/**
 * Transform uint8 into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
inline std::string to8(uint8_t data, bool prefix = false) throw(std::bad_alloc) { return to<uint8_t>(data, prefix); }

/**
 * Transform uint16 into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
inline std::string to16(uint16_t data, bool prefix = false) throw(std::bad_alloc)
{
	return to<uint16_t>(data, prefix);
}

/**
 * Transform uint24 into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string to24(uint32_t data, bool prefix = false) throw(std::bad_alloc);

/**
 * Transform uint32 into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
inline std::string to32(uint32_t data, bool prefix = false) throw(std::bad_alloc)
{
	return to<uint32_t>(data, prefix);
}

/**
 * Transform uint64 into full-width hexadecimal.
 *
 * Parameter data: The number to transform.
 * Parameter prefix: Add the '0x' prefix if true.
 * Returns: The hex string.
 * Throws std::bad_alloc: Not enough memory.
 */
inline std::string to64(uint64_t data, bool prefix = false) throw(std::bad_alloc)
{
	return to<uint64_t>(data, prefix);
}

/**
 * Transform hexadecimal into binary.
 *
 * Parameter buf: Buffer to write binary to. The size needs to be half of the hexadecimal string, rounded up.
 * Parameter hex: The hexadecimal string.
 * Throws std::runtime_error: Bad hexadecimal character in string.
 */
void b_from(uint8_t* buf, const std::string& hex) throw(std::runtime_error);

/**
 * Transform hexadecimal into unsigned integer.
 *
 * Parameter hex: The hexadecimal string.
 * Throws std::runtime_error: Bad hexadecimal character in string.
 */
template<typename T> T from(const std::string& hex) throw(std::runtime_error);
}

#endif
