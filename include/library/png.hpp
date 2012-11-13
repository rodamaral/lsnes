#ifndef _library__png__hpp__included__
#define _library__png__hpp__included__

#include <stdexcept>
#include <cstdint>
#include <string>

/**
 * Save a PNG.
 *
 * parameter file: Filename to save to.
 * parameter data24: 3 elements per pixel (r,g, b) per pixel, left-to-right, top-to-bottom order.
 * parameter width: Width of the image.
 * parameter height: Height of the image.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error saving PNG.
 */
void save_png_data(const std::string& file, uint8_t* data24, uint32_t width, uint32_t height) throw(std::bad_alloc,
	std::runtime_error);

#endif
