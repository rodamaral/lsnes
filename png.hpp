#ifndef _png__hpp__included__
#define _png__hpp__included__

#include <stdexcept>
#include <cstdint>
#include <string>

void save_png_data(const std::string& file, uint8_t* data24, uint32_t width, uint32_t height) throw(std::bad_alloc,
	std::runtime_error);

#endif