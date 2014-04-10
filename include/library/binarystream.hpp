#ifndef _library__binarystream__hpp__included__
#define _library__binarystream__hpp__included__

#include <iostream>
#include <sstream>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace binarystream
{
/**
 * Output binary stream.
 */
struct output
{
public:
/**
 * Create a new output binary stream outputting to internal buffer.
 */
	output();
/**
 * Create a new output binary stream outputting to given output stream.
 *
 * Parameter s: The stream to output to.
 */
	output(int s);
/**
 * Output a byte to stream (1 byte).
 *
 * Parameter byte: The byte to output.
 */
	void byte(uint8_t byte);
/**
 * Output a number to stream.
 *
 * Parameter number: The number to output.
 */
	void number(uint64_t number);
/**
 * Count number of bytes needed to store given number.
 *
 * Parameter number: The number to query.
 * Return: The number of bytes needed.
 */
	size_t numberbytes(uint64_t number);
/**
 * Count number of bytes needed to store given string.
 *
 * Parameter string: The string to query.
 * Return: The number of bytes needed.
 */
	size_t stringbytes(const std::string& string);
/**
 * Output a 32-bit number to stream (4 byte).
 *
 * Parameter number: The number to output.
 */
	void number32(uint32_t number);
/**
 * Output a string with explicit length indication to the stream.
 *
 * This takes space for one number + number of bytes in the string.
 *
 * Parameter string: The number to output.
 */
	void string(const std::string& string);
/**
 * Output a string without length indication to the stream.
 *
 * This takes space for number of bytes in the string.
 *
 * Parameter string: The number to output.
 */
	void string_implicit(const std::string& string);
/**
 * Output a octet string without length indication to the stream.
 *
 * This takes space for number of bytes in the sequence.
 *
 * Parameter blob: The octet string to output.
 */
	void blob_implicit(const std::vector<char>& blob);
/**
 * Output a octet string without length indication to the stream.
 *
 * This takes space for number of bytes in the sequence.
 *
 * Parameter buf: The input buffer to read the octet string from.
 * Parameter bufsize: The size of buffer in bytes.
 */
	void raw(const void* buf, size_t bufsize);
/**
 * Output a extension substream into stream.
 *
 * Parameter tag: Tag identifying the extension member type.
 * Parameter fn: Function writing the contents of the extension substream.
 * Parameter even_empty: If true, the member is written even if empty (otherwise it is elided).
 */
	void extension(uint32_t tag, std::function<void(output&)> fn, bool even_empty = false);
/**
 * Output a extension substream with known size into stream.
 *
 * In exchange for having to know the size of the payload, this is faster to write as it avoids buffering.
 *
 * Parameter tag: Tag identifying the extension member type.
 * Parameter fn: Function writing the contents of the extension substream.
 * Parameter even_empty: If true, the member is written even if empty (otherwise it is elided).
 * Parameter size_precognition: The known size of the payload.
 */
	void extension(uint32_t tag, std::function<void(output&)> fn, bool even_empty,
		size_t size_precognition);
/**
 * Output explicit extension tag.
 *
 * This has to be followed by writing size bytes, forming the payload.
 *
 * Parameter tag: The Tag identifying the extension member type.
 * Parameter size: Size of the payload.
 */
	void write_extension_tag(uint32_t tag, uint64_t size);
/**
 * Get the output stream contents.
 *
 * The output stream has to use internal buffer for this to work.
 *
 * Returns: The internal buffer contents.
 */
	std::string get();
private:
	inline void write(const char* buf, size_t size);
	int strm;
	std::vector<char> buf;
};

/**
 * Input binary stream.
 */
struct input
{
public:
/**
 * Extension tag handler.
 */
	struct binary_tag_handler
	{
/**
 * The extension tag to activate on.
 */
		uint32_t tag;
/**
 * Handler function for the tag.
 *
 * Parameter s: The substream of extension.
 */
		std::function<void(input& s)> fn;
	};
/**
 * Create a new top-level input stream, reading from specified stream.
 */
	input(int s);
/**
 * Create a new input substream, under specified top-level stream and with specified length.
 *
 * Parameter s: The top-level stream.
 * Parameter len: Amount of payload in extension stream.
 */
	input(input& s, uint64_t len);
/**
 * Read a byte.
 *
 * Returns: The read byte.
 */
	uint8_t byte();
/**
 * Read a number.
 *
 * Returns: The read number.
 */
	uint64_t number();
/**
 * Read a 32-bit number.
 *
 * Returns: The read number.
 */
	uint32_t number32();
/**
 * Read a string with explicit length indication.
 *
 * Returns: The read string.
 */
	std::string string();
/**
 * Read a string without explicit length indication, quitting when reaching end of extension substream.
 *
 * Can be only used in extension substreams.
 *
 * Returns: The read string.
 */
	std::string string_implicit();
/**
 * Read a octet string without explicit length indication, quitting when reaching end of extension substream.
 *
 * Can be only used in extension substreams.
 *
 * Parameter blob: Store the read octet string here.
 */
	void blob_implicit(std::vector<char>& blob);
/**
 * Read a octet string of specified length.
 *
 * Parameter buf: Buffer to store the octet string to.
 * Parameter bufsize: The amount of data to read.
 */
	void raw(void* buf, size_t bufsize);
/**
 * Read extension substreams.
 *
 * This reads substreams until the end of stream.
 *
 * Parameter fn: Function handling the read extension substream.
 *	Parameter tag: The type tag of read substream.
 *	Parameter s: The substream with contents of extension substream.
 */
	void extension(std::function<void(uint32_t tag, input& s)> fn);
/**
 * Read extension substreams.
 *
 * This reads substreams until the end of stream.
 *
 * Parameter funcs: Table of functions to call for each known extension type.
 * Parameter default_hdlr: Handler for unknown types (parameters as in other ::extension()).
 */
	void extension(std::initializer_list<binary_tag_handler> funcs,
		std::function<void(uint32_t tag, input& s)> default_hdlr);
/**
 * Get number of bytes left in substream.
 *
 * Can only be used for substreams.
 *
 * Returns: Number of bytes left.
 */
	uint64_t get_left()
	{
		if(!parent)
			throw std::logic_error("binarystream::input::get_left() can only be used in substreams");
		return left;
	}
private:
	bool read(char* buf, size_t size, bool allow_none = false);
	void flush();
	input* parent;
	int strm;
	uint64_t left;
};

/**
 * Do nothing. Handy as second parameter for two-parameter output::extension() if the table enumerates all non-
 * ignored types.
 */
void null_default(uint32_t tag, input& s);
}

#endif
