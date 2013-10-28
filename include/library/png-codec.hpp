#ifndef _library__png__hpp__included__
#define _library__png__hpp__included__

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

struct png_decoded_image
{
	png_decoded_image();
	png_decoded_image(std::istream& file);
	png_decoded_image(const std::string& file);
	size_t width;
	size_t height;
	bool has_palette;
	std::vector<uint32_t> data;
	std::vector<uint32_t> palette;
private:
	void decode_png(std::istream& file);
};

struct png_encodedable_image
{
	png_encodedable_image();
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

#endif
