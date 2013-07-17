#ifndef _library__png_decoder__hpp__included__
#define _library__png_decoder__hpp__included__

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>

struct png_decoded_image
{
	size_t width;
	size_t height;
	bool has_palette;
	std::vector<uint32_t> data;
	std::vector<uint32_t> palette;
};

void decode_png(const std::string& file, png_decoded_image& out);

#endif
