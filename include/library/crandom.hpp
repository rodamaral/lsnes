#ifndef _library__random__hpp__included__
#define _library__random__hpp__included__

#include <cstdlib>

namespace crandom
{
/**
 * Initialize random number generator.
 *
 * Throws std::runtime_error: Can't initialize RNG.
 */
	void init();
/**
 * Generate random bits. Automatically initializes the generator if not already initialized.
 *
 * Parameter buffer: The buffer to fill.
 * Parameter buffersize: Number of bytes to fill.
 * Throws std::runtime_error: Can't initialize RNG.
 */
	void generate(void* buffer, size_t buffersize);
}

#endif
