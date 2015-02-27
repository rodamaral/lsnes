#ifndef _random__hpp__included__
#define _random__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include "library/string.hpp"

/**
 * \brief Get random hexes
 *
 * Get string of random hex characters of specified length.
 *
 * \param length The number of hex characters to return.
 * \return The random hexadecimal string.
 * \throws std::bad_alloc Not enough memory.
 */
std::string get_random_hexstring(size_t length) throw(std::bad_alloc);

/**
 * \brief Set random seed
 *
 * This function sets the random seed to use.
 *
 * \param seed The value to use as seed.
 * \throw std::bad_alloc Not enough memory.
 */
void set_random_seed(const std::string& seed) throw(std::bad_alloc);

/**
 * \brief Set random seed to (hopefully) unique value
 *
 * This function sets the random seed to value that should only be used once. Note, the value is not necressarily
 * crypto-secure, even if it is unique.
 *
 * \throw std::bad_alloc Not enough memory.
 */
void set_random_seed() throw(std::bad_alloc);

/**
 * Mix some entropy.
 */
void random_mix_timing_entropy();

/**
 * 256 bits of as high quality entropy as possible.
 */
void highrandom_256(uint8_t* buf);

/**
 * Contribute buffer of entropy.
 */
void contribute_random_entropy(void* buf, size_t bytes);

#endif
