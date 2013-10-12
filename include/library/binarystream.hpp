#ifndef _library__binarystream__hpp__included__
#define _library__binarystream__hpp__included__

#include <iostream>
#include <sstream>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

struct binary_output_stream
{
public:
	binary_output_stream();
	binary_output_stream(std::ostream& s);
	void byte(uint8_t byte);
	void number(uint64_t number);
	size_t numberbytes(uint64_t number);
	void number32(uint32_t number);
	void string(const std::string& string);
	void string_implicit(const std::string& string);
	void blob_implicit(const std::vector<char>& blob);
	void raw(const void* buf, size_t bufsize);
	void extension(uint32_t tag, std::function<void(binary_output_stream&)> fn, bool even_empty = false);
	void extension(uint32_t tag, std::function<void(binary_output_stream&)> fn, bool even_empty,
		size_t size_precognition);
	void write_extension_tag(uint32_t tag, uint64_t size);
	std::string get();
private:
	std::ostream& strm;
	std::ostringstream buf;
};

struct binary_input_stream
{
public:
	struct binary_tag_handler
	{
		uint32_t tag;
		std::function<void(binary_input_stream& s)> fn;
	};
	binary_input_stream(std::istream& s);
	binary_input_stream(binary_input_stream& s, uint64_t len);
	uint8_t byte();
	uint64_t number();
	uint32_t number32();
	std::string string();
	std::string string_implicit();
	void blob_implicit(std::vector<char>& blob);
	void raw(void* buf, size_t bufsize);
	void extension(std::function<void(uint32_t tag, binary_input_stream& s)> fn);
	void extension(std::initializer_list<binary_tag_handler> funcs,
		std::function<void(uint32_t tag, binary_input_stream& s)> default_hdlr);
	uint64_t get_left()
	{
		if(!parent)
			throw std::logic_error("binary_input_stream::get_left() can only be used in substreams");
		return left;
	}
private:
	bool read(char* buf, size_t size, bool allow_none = false);
	void flush();
	binary_input_stream* parent;
	std::istream& strm;
	uint64_t left;
};

void binary_null_default(uint32_t tag, binary_input_stream& s);

#endif
