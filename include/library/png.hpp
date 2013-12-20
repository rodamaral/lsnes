#ifndef _library__png__hpp__included__
#define _library__png__hpp__included__

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

namespace png
{
struct decoder
{
	decoder();
	decoder(std::istream& file);
	decoder(const std::string& file);
	size_t width;
	size_t height;
	bool has_palette;
	std::vector<uint32_t> data;
	std::vector<uint32_t> palette;
private:
	void decode_png(std::istream& file);
};

struct encoder
{
	encoder();
	size_t width;
	size_t height;
	bool has_palette;
	bool has_alpha;
	uint32_t colorkey;
	std::vector<uint32_t> data;
	std::vector<uint32_t> palette;
	void encode(const std::string& file) const;
	void encode(std::ostream& file) const;
};
}

#endif
