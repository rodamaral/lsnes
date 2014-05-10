#ifndef _library__random__hpp__included__
#define _library__random__hpp__included__

#include <cstdlib>
#include <cstdint>

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
/**
 * Get random number from arch-specific RNG.
 *
 * Returns: Random number, or 0 if not supported.
 *
 * Note: Beware, the random numbers might be weak.
 */
	uint64_t arch_get_random();
/**
 * Get processor TSC.
 *
 * Returns: TSC, or 0 if not supported.
 */
	uint64_t arch_get_tsc();
}

#endif
